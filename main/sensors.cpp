#include "sensors.h"
#include "pins.h"
#include <Arduino.h>

Adafruit_INA260 ina260;

namespace {
constexpr float ADC_REF_V = 3.3f;
constexpr float ADC_MAX = 4095.0f;
}

void initSensors(AppState& state) {
  pinMode(ALT_TEMP_PIN, INPUT);
  state.ina260Available = ina260.begin();
}

float readAlternatorTemperatureC() {
  const int raw = analogRead(ALT_TEMP_PIN);
  const float voltage = raw * (ADC_REF_V / ADC_MAX);
  return (voltage - 0.5f) * 100.0f;
}

void sampleSensors(AppState& state) {
  state.alternatorTempC = readAlternatorTemperatureC();

  if (state.ina260Available) {
    state.chargeCurrent = ina260.readCurrent() / 1000.0f;
    state.busVoltage = ina260.readBusVoltage() / 1000.0f;
  }
}
