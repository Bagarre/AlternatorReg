#include "sensors.h"
#include "pins.h"
#include "log_buffer.h"
#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>

namespace {
constexpr uint8_t INA226_I2C_ADDR = 0x40;
constexpr float SHUNT_RESISTANCE_OHM = 0.0001f; // 500A/50mV shunt. Change if needed.
constexpr float CURRENT_ZERO_DEADBAND_A = 0.05f;
constexpr uint8_t DS18B20_ALT_INDEX = 0;
constexpr uint8_t DS18B20_ENGINE_ROOM_INDEX = 1;

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature tempSensors(&oneWire);

class INA226Simple {
public:
  explicit INA226Simple(uint8_t address) : addr(address) {}

  bool begin() {
    Wire.begin();
    // AVG=16 samples, VBUSCT=1.1ms, VSHCT=1.1ms, mode=shunt+bus continuous.
    writeRegister16(0x00, 0x4527);
    delay(5);
    uint16_t bus = 0;
    return readRegister16(0x02, bus);
  }

  bool read(float& busVoltageV, float& shuntVoltageMV, float& currentA) {
    uint16_t rawBus = 0;
    uint16_t rawShuntU = 0;
    if (!readRegister16(0x02, rawBus)) return false;
    if (!readRegister16(0x01, rawShuntU)) return false;

    const int16_t rawShunt = static_cast<int16_t>(rawShuntU);
    busVoltageV = rawBus * 0.00125f;
    shuntVoltageMV = rawShunt * 0.0025f;
    currentA = (shuntVoltageMV / 1000.0f) / SHUNT_RESISTANCE_OHM;

    if (fabs(currentA) < CURRENT_ZERO_DEADBAND_A) {
      currentA = 0.0f;
    }
    return true;
  }

private:
  uint8_t addr;

  bool writeRegister16(uint8_t reg, uint16_t value) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(static_cast<uint8_t>(value >> 8));
    Wire.write(static_cast<uint8_t>(value & 0xFF));
    return Wire.endTransmission() == 0;
  }

  bool readRegister16(uint8_t reg, uint16_t& value) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;

    if (Wire.requestFrom(static_cast<int>(addr), 2) != 2) return false;
    const uint8_t msb = Wire.read();
    const uint8_t lsb = Wire.read();
    value = (static_cast<uint16_t>(msb) << 8) | lsb;
    return true;
  }
};

INA226Simple ina226(INA226_I2C_ADDR);

bool prevInaOk = false;
bool prevAltTempOk = false;
bool prevEngineRoomTempOk = false;
}

bool maintenanceMode() {
  return digitalRead(MAINTENANCE_MODE_PIN) == LOW;
}

bool validDs18b20Temp(float tempC) {
  // DEVICE_DISCONNECTED_C is normally -127C. 85C is the DS18B20 power-on value
  // before a real conversion completes, so do not count it as valid.
  return isfinite(tempC) && tempC != DEVICE_DISCONNECTED_C && tempC != 85.0f && tempC > -55.0f && tempC < 125.0f;
}

void initSensors(AppState& state) {
  pinMode(MAINTENANCE_MODE_PIN, INPUT_PULLUP);

  Wire.begin();
  state.ina226Available = ina226.begin();

  tempSensors.begin();
  tempSensors.setWaitForConversion(false);
  tempSensors.requestTemperatures();
  delay(750);

  prevInaOk = state.ina226Available;
  prevAltTempOk = false;
  prevEngineRoomTempOk = false;
}

void sampleSensors(AppState& state) {
  float busV = NAN;
  float shuntMV = NAN;
  float currentA = NAN;

  state.ina226Available = ina226.read(busV, shuntMV, currentA);
  if (state.ina226Available) {
    state.busVoltage = busV;
    state.shuntVoltageMV = shuntMV;
    state.chargeCurrent = currentA;
  } else {
    state.busVoltage = NAN;
    state.shuntVoltageMV = NAN;
    state.chargeCurrent = NAN;
  }

  state.alternatorTempC = tempSensors.getTempCByIndex(DS18B20_ALT_INDEX);
  state.engineRoomTempC = tempSensors.getTempCByIndex(DS18B20_ENGINE_ROOM_INDEX);
  state.altTempSensorOk = validDs18b20Temp(state.alternatorTempC);
  state.engineRoomTempSensorOk = validDs18b20Temp(state.engineRoomTempC);

  tempSensors.requestTemperatures();

  if (state.ina226Available != prevInaOk) {
    addLog(state.ina226Available ? "INA226 OK" : "INA226 missing");
    prevInaOk = state.ina226Available;
  }
  if (state.altTempSensorOk != prevAltTempOk) {
    addLog(state.altTempSensorOk ? "Alternator temp sensor OK" : "Alternator temp sensor missing");
    prevAltTempOk = state.altTempSensorOk;
  }
  if (state.engineRoomTempSensorOk != prevEngineRoomTempOk) {
    addLog(state.engineRoomTempSensorOk ? "Engine room temp sensor OK" : "Engine room temp sensor missing");
    prevEngineRoomTempOk = state.engineRoomTempSensorOk;
  }
}
