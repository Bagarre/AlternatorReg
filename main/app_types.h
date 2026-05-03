#pragma once

#include <Arduino.h>

struct AppConfig {
  // Target voltage comes from config/UI for now. In boat use this can be
  // overwritten by DCVV/Cerbo logic elsewhere. This controller does NOT run
  // bulk/absorb/float stages locally.
  float targetVoltage = 14.4f;
  float currentLimit = 100.0f;

  // PID terms for voltage regulation. Start conservative.
  float pidKp = 45.0f;
  float pidKi = 4.0f;
  float pidKd = 0.0f;

  // Alternator thermal derating curve. Full output below derateStartTempC,
  // linearly reduced to zero by derateStopTempC.
  float derateStartTempC = 82.0f;
  float derateStopTempC = 96.0f;

  // Field soft-start and engine RPM gate.
  float softStartSeconds = 60.0f;
  float minStartRPM = 2200.0f;
  float rpmHoldSeconds = 300.0f;

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

  bool ina226Available = false;
  bool altTempSensorOk = false;
  bool engineRoomTempSensorOk = false;
  bool startupCheckComplete = false;
  bool startupCheckOk = false;

  float currentRPM = 0.0f;
  float alternatorTempC = NAN;
  float engineRoomTempC = NAN;
  float busVoltage = NAN;
  float chargeCurrent = NAN;
  float shuntVoltageMV = NAN;

  uint8_t pwmDuty = 0;
  uint8_t requestedDuty = 0;
  uint8_t pidRequestedDuty = 0;
  uint8_t tempDutyCap = 255;
  uint8_t currentDutyCap = 255;
  uint8_t softStartDutyCap = 255;

  float pidIntegral = 0.0f;
  float lastPidError = 0.0f;

  unsigned long bootMillis = 0;
  unsigned long lastCANMillis = 0;
  unsigned long lastControlMillis = 0;
  unsigned long lastStatusSampleMillis = 0;
  unsigned long rpmAboveThresholdStartMillis = 0;
  unsigned long regulationStartMillis = 0;

  String stage = "Idle";
  String lastDisableReason = "None";
  String canStatus = "No CAN";
  String startupStatus = "Booting";
};
