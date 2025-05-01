/*
 * ---------------------------------------------------------------
 * Smart Alternator Regulator (Feather M4 CAN)
 * ---------------------------------------------------------------
 * Description:
 *   Controls a 12V alternator field using input from NMEA 2000 and
 *   local sensors. Monitors engine RPM, BMS charge permission, and
 *   temperature to enable/disable alternator charging intelligently.
 *
 * Features:
 *   - Enables charging only above a configured RPM threshold
 *   - Requires RPM to be held above threshold for a duration
 *   - Accepts engine RPM via PGN 127488 (Engine Parameters)
 *   - Tracks BMS and alternator temperature states (via flags)
 *   - Outputs charge enable signal to alternator field (with soft-start)
 *   - Disables field if CAN messages are lost
 *
 * Author: David Ross
 * Date: 30 April 2025
 * Version: 1.1
 * License: MIT
 * ---------------------------------------------------------------
 */

#include <FlexCAN_T4.h>
FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can0;

const int FIELD_PIN = 5;
bool bmsAllowsCharge = true;
bool alternatorOverTemp = false;

float currentRPM = 0;
unsigned long rpmAboveThresholdStartMillis = 0;
unsigned long lastRPMMillis = 0;
unsigned long lastCANMillis = 0;

bool fieldEnabled = false;

/**
 * setup()
 * Initializes CAN bus and field control pin.
 */
void setup() {
  pinMode(FIELD_PIN, OUTPUT);
  analogWrite(FIELD_PIN, 0);
  Serial.begin(9600);
  Can0.begin();
  Can0.setBaudRate(250000);
}

/**
 * loop()
 * Evaluates whether charging is allowed and applies soft start.
 */


void loop() {
  alternatorTempC = readAlternatorTemperature();

  // Periodically broadcast alternator temperature over CAN
  if (millis() - lastTempSend > 1000) {
    sendAlternatorTempCAN();
    lastTempSend = millis();
  }

  alternatorTempC = readAlternatorTemperature();

  static unsigned long lastUpdate = 0;
  unsigned long now = millis();

  processCANMessages();

  // CAN watchdog: turn off field if comms lost
  if (now - lastCANMillis > 3000) {
    disableField("CAN timeout");
    return;
  }

  // Check every 1s whether to (re)enable field
  if (now - lastUpdate > 1000) {
    lastUpdate = now;

    bool allow = canCharge();
    Serial.print("RPM: ");
    Serial.print(currentRPM);
    Serial.print(" | Charge OK: ");
    Serial.println(allow ? "YES" : "NO");

    if (allow && !fieldEnabled) {
      enableFieldSoftStart();
    } else if (!allow && fieldEnabled) {
      disableField("Conditions not met");
    }
  }
}

/**
 * canCharge()
 * Returns true if all conditions for safe charging are met.
 */
bool canCharge() {
  const int cruiseRPMThreshold = 1200;
  const unsigned long rpmHoldDuration = 30000;
  unsigned long now = millis();

  if (currentRPM > cruiseRPMThreshold) {
    if (rpmAboveThresholdStartMillis == 0) {
      rpmAboveThresholdStartMillis = now;
    }
  } else {
    rpmAboveThresholdStartMillis = 0;
  }

  bool rpmOK = currentRPM > cruiseRPMThreshold &&
               (now - rpmAboveThresholdStartMillis > rpmHoldDuration);

  return rpmOK && bmsAllowsCharge && !alternatorOverTemp;
}

/**
 * processCANMessages()
 * Reads PGN 127488 and updates engine RPM.
 */
void processCANMessages() {
  CAN_message_t msg;
  while (Can0.read(msg)) {
    lastCANMillis = millis();

    if (msg.id == 127488 && msg.len >= 8) {
      uint16_t rpmRaw = msg.buf[3] | (msg.buf[4] << 8);
      currentRPM = rpmRaw * 0.25;
      lastRPMMillis = millis();
    }
  }
}

/**
 * enableFieldSoftStart()
 * Gradually ramps up the field to reduce startup shock and apply derating if needed.
 * Derates above 80°C and cuts off field above 95°C.
 */

void enableFieldSoftStart() {
  Serial.println("Enabling field (soft start)");
  const int steps = 20;
  const int maxPWM = 255;

  for (int i = 0; i <= steps; i++) {
    int pwm = (i * maxPWM) / steps;

// Apply derating based on alternator temperature
if (alternatorTempC >= 95.0) {
  pwm = 0;
} else if (alternatorTempC >= 80.0) {
  float factor = 1.0 - ((alternatorTempC - 80.0) / 15.0);  // Linear scale down
  pwm *= factor;
}

    analogWrite(FIELD_PIN, pwm);
    delay(100); // ~2 seconds total
  }
  fieldEnabled = true;
}

/**
 * disableField(reason)
 * Turns off the alternator field.
 */
void disableField(const char* reason) {
  analogWrite(FIELD_PIN, 0);
  if (fieldEnabled) {
    Serial.print("Disabling field: ");
    Serial.println(reason);
  }
  fieldEnabled = false;
}


/**
 * readAlternatorTemperature()
 * Reads analog voltage from TMP36-style sensor and converts to Celsius.
 */
float readAlternatorTemperature() {
  int raw = analogRead(ALT_TEMP_PIN);
  float voltage = raw * (3.3 / 1023.0);      // for 3.3V Feather boards
  float tempC = (voltage - 0.5) * 100.0;     // TMP36 formula
  return tempC;
}


/**
 * sendAlternatorTempCAN()
 * Sends alternator temperature (in °C * 100) as a 2-byte integer over CAN.
 */
void sendAlternatorTempCAN() {
  int16_t tempRaw = alternatorTempC * 100;  // scale for 0.01°C precision

  CAN_message_t msg;
  msg.id = PGN_ALT_TEMP;
  msg.len = 2;
  msg.buf[0] = tempRaw & 0xFF;
  msg.buf[1] = (tempRaw >> 8) & 0xFF;

  Can0.write(msg);
}
