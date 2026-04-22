#pragma once

#include <Arduino.h>

struct AppConfig {
  float targetVoltage = 14.4f;
  float currentLimit = 100.0f;
  float floatVoltage = 13.6f;
  float derateTempC = 82.0f;
  bool canInput = true;
  String wifiSSID;
  String wifiPassword;
};

struct AppState {
  bool systemEnabled = true;
  bool bmsAllowsCharge = true;
  bool alternatorOverTemp = false;
  bool fieldEnabled = false;
  bool canOnline = false;
  bool ina260Available = false;

  float currentRPM = 0.0f;
  float alternatorTempC = 0.0f;
  float engineRoomTempC = NAN;
  float busVoltage = 0.0f;
  float chargeCurrent = 0.0f;

  uint8_t pwmDuty = 0;
  uint8_t requestedDuty = 0;

  unsigned long bootMillis = 0;
  unsigned long lastCANMillis = 0;
  unsigned long lastControlMillis = 0;
  unsigned long lastStatusSampleMillis = 0;
  unsigned long rpmAboveThresholdStartMillis = 0;

  String stage = "Idle";
  String lastDisableReason = "None";
  String canStatus = "No CAN";
};
