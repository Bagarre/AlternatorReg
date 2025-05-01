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
 *   - Outputs charge enable signal to alternator field
 *
 * CAN Messaging:
 *   - PGN 127488: Engine Speed input (used to control charging logic)
 *
 * Author: David Ross
 * Date: 30 April 2025
 * Version: 1.0
 * License: MIT
 * ---------------------------------------------------------------
 */


#include <FlexCAN_T4.h>
FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can0;

// Output pin that controls the alternator field (charge enable)
const int FIELD_PIN = 5;

// System conditions that impact charging
bool bmsAllowsCharge = true;
bool alternatorOverTemp = false;

// Engine RPM tracking via NMEA 2000 (PGN 127488)
float currentRPM = 0;                         // Current engine RPM
unsigned long rpmAboveThresholdStartMillis = 0;  // Timestamp when RPM exceeded threshold
unsigned long lastRPMMillis = 0;              // Time last valid RPM was received

/**
 * setup()
 * Initializes I/O pins, serial output, and CAN bus interface.
 */
void setup() {
  pinMode(FIELD_PIN, OUTPUT);                 // Alternator control output
  digitalWrite(FIELD_PIN, LOW);               // Default to not charging
  Serial.begin(9600);                         // Serial monitor for debug
  Can0.begin();                               // Initialize CAN interface
  Can0.setBaudRate(250000);                   // Standard NMEA 2000 bitrate
}

/**
 * loop()
 * Runs every cycle to process CAN messages and determine whether to enable alternator charging.
 */
void loop() {
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();

  // Continuously process incoming CAN data (engine RPM, etc.)
  processCANMessages();

  // Check every second if alternator should be charging
  if (now - lastUpdate > 1000) {
    lastUpdate = now;

    bool chargingAllowed = canCharge();
    digitalWrite(FIELD_PIN, chargingAllowed ? HIGH : LOW); // Enable or disable charging

    // Debug output
    Serial.print("RPM: ");
    Serial.print(currentRPM);
    Serial.print(" | Charging: ");
    Serial.println(chargingAllowed ? "ENABLED" : "DISABLED");
  }
}

/**
 * canCharge()
 * Returns true if all charging conditions are met:
 *  - RPM has exceeded cruise threshold for enough time
 *  - BMS allows charge
 *  - Alternator is not over temperature
 */
bool canCharge() {
  const int cruiseRPMThreshold = 1200;               // Minimum RPM to allow charging
  const unsigned long rpmHoldDuration = 30000;       // Required duration above RPM (30 sec)
  unsigned long now = millis();

  // Track when RPM first rises above threshold
  if (currentRPM > cruiseRPMThreshold) {
    if (rpmAboveThresholdStartMillis == 0) {
      rpmAboveThresholdStartMillis = now;
    }
  } else {
    rpmAboveThresholdStartMillis = 0;
  }

  // Allow charging only if RPM has been high long enough and other conditions are OK
  bool rpmOK = currentRPM > cruiseRPMThreshold &&
               (now - rpmAboveThresholdStartMillis > rpmHoldDuration);

  return rpmOK && bmsAllowsCharge && !alternatorOverTemp;
}

/**
 * processCANMessages()
 * Reads CAN messages and extracts engine RPM from PGN 127488.
 * RPM is decoded from bytes 4–5 (LSB first), scaled in 0.25 RPM units.
 */
void processCANMessages() {
  CAN_message_t msg;
  while (Can0.read(msg)) {
    if (msg.id == 127488 && msg.len >= 8) {
      uint16_t rpmRaw = msg.buf[3] | (msg.buf[4] << 8); // Little-endian
      currentRPM = rpmRaw * 0.25;                        // Scale factor
      lastRPMMillis = millis();                          // Track last valid update
    }
  }
}
