#pragma once

// ESP32 DevKit V1 + external TJA1051T/3 CAN transceiver pin map.
// Adjust here if your board wiring changes.
//
// IMPORTANT:
// - ESP32 default I2C pins are SDA=21 and SCL=22.
// - Do NOT use GPIO21/22 for CAN when using INA226 on I2C.
// - TJA1051T/3 STB/standby pin must be tied LOW/GND for normal operation.

// Low-side MOSFET gate drive for alternator field PWM.
constexpr int FIELD_PIN = 25;

// OneWire bus for both DS18B20 sensors:
//   index 0 = alternator case temp
//   index 1 = engine room ambient temp
constexpr int ONE_WIRE_PIN = 26;

// DIP switch to GND enables verbose maintenance logging over USB serial.
constexpr int MAINTENANCE_MODE_PIN = 27;

// INA226 I2C bus pins.
constexpr int I2C_SDA_PIN = 21;
constexpr int I2C_SCL_PIN = 22;

// ESP32 TWAI/CAN pins to external TJA1051T/3 transceiver.
// Wiring:
//   GPIO5 -> TJA1051 TXD
//   GPIO4 -> TJA1051 RXD
//   TJA1051 CANH/CANL -> NMEA2000 backbone
//   TJA1051 STB -> GND
constexpr int CAN_TX_PIN = 5;
constexpr int CAN_RX_PIN = 4;

constexpr int PWM_CHANNEL = 0;
constexpr int PWM_FREQ_HZ = 250;
constexpr int PWM_RESOLUTION_BITS = 8;
