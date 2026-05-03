#pragma once

// ESP32 pin map. Adjust here if your board wiring changes.
constexpr int FIELD_PIN = 25;

// OneWire bus for both DS18B20 sensors:
//   index 0 = alternator case temp
//   index 1 = engine room ambient temp
constexpr int ONE_WIRE_PIN = 26;

// DIP switch to GND enables verbose maintenance logging over USB serial.
constexpr int MAINTENANCE_MODE_PIN = 27;

constexpr int CAN_TX_PIN = 22;
constexpr int CAN_RX_PIN = 21;

constexpr int PWM_CHANNEL = 0;
constexpr int PWM_FREQ_HZ = 250;
constexpr int PWM_RESOLUTION_BITS = 8;
