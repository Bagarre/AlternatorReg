#include "control_loop.h"
#include "field_driver.h"
#include "log_buffer.h"
#include <Arduino.h>

namespace {
constexpr unsigned long CONTROL_INTERVAL_MS = 100;
constexpr uint8_t DUTY_MAX = 255;
constexpr uint8_t DUTY_RISE_PER_TICK = 2;
constexpr uint8_t DUTY_FALL_PER_TICK = 8;
constexpr unsigned long RPM_HOLD_MS = 30000;
constexpr float RPM_ENABLE_THRESHOLD = 1200.0f;
constexpr float HARD_OVER_TEMP_MARGIN_C = 5.0f;
constexpr float VOLTAGE_DEADBAND = 0.05f;
constexpr float TEMP_DERATE_START_MARGIN_C = 8.0f;

uint8_t clampDuty(int value) {
  if (value < 0) return 0;
  if (value > DUTY_MAX) return DUTY_MAX;
  return static_cast<uint8_t>(value);
}

uint8_t applyRateLimit(uint8_t currentDuty, uint8_t requestedDuty) {
  if (requestedDuty > currentDuty) {
    return static_cast<uint8_t>(min<int>(currentDuty + DUTY_RISE_PER_TICK, requestedDuty));
  }
  return static_cast<uint8_t>(max<int>(currentDuty - DUTY_FALL_PER_TICK, requestedDuty));
}

bool rpmAllowsCharge(const AppConfig& config, AppState& state) {
  const unsigned long now = millis();

  if (!config.canInput) {
    return true;
  }

  if (state.currentRPM > RPM_ENABLE_THRESHOLD) {
    if (state.rpmAboveThresholdStartMillis == 0) {
      state.rpmAboveThresholdStartMillis = now;
    }
  } else {
    state.rpmAboveThresholdStartMillis = 0;
  }

  if (state.rpmAboveThresholdStartMillis == 0) {
    return false;
  }

  return (now - state.rpmAboveThresholdStartMillis) >= RPM_HOLD_MS;
}

uint8_t voltageRequestDuty(const AppConfig& config, const AppState& state, uint8_t currentDuty) {
  int requested = currentDuty;
  const float error = config.targetVoltage - state.busVoltage;

  if (error > VOLTAGE_DEADBAND) {
    requested += 3;
  } else if (error < -VOLTAGE_DEADBAND) {
    requested -= 4;
  }

  return clampDuty(requested);
}

uint8_t currentCapDuty(const AppConfig& config, const AppState& state, uint8_t currentDuty) {
  if (state.chargeCurrent <= config.currentLimit) {
    return DUTY_MAX;
  }

  const float over = state.chargeCurrent - config.currentLimit;
  int capped = currentDuty - static_cast<int>(6 + over * 1.5f);
  return clampDuty(capped);
}

uint8_t tempCapDuty(const AppConfig& config, const AppState& state, uint8_t currentDuty) {
  const float derateStart = config.derateTempC - TEMP_DERATE_START_MARGIN_C;

  if (state.alternatorTempC >= config.derateTempC + HARD_OVER_TEMP_MARGIN_C) {
    return 0;
  }

  if (state.alternatorTempC <= derateStart) {
    return DUTY_MAX;
  }

  const float span = (config.derateTempC - derateStart);
  const float ratio = (config.derateTempC - state.alternatorTempC) / max(0.1f, span);
  const int cap = static_cast<int>(currentDuty * constrain(ratio, 0.15f, 1.0f));
  return clampDuty(cap);
}
}

void initControlLoop() {
}

void disableField(AppState& state, const String& reason) {
  disableFieldOutput();
  state.pwmDuty = 0;
  state.requestedDuty = 0;
  if (state.fieldEnabled || state.lastDisableReason != reason) {
    addLog("Field disabled: " + reason, state.busVoltage, state.chargeCurrent, state.alternatorTempC);
  }
  state.fieldEnabled = false;
  state.stage = "Idle";
  state.lastDisableReason = reason;
}

void runControlLoop(const AppConfig& config, AppState& state) {
  const unsigned long now = millis();
  if (now - state.lastControlMillis < CONTROL_INTERVAL_MS) {
    return;
  }
  state.lastControlMillis = now;

  if (!state.systemEnabled) {
    disableField(state, "System disabled");
    return;
  }

  if (!state.bmsAllowsCharge) {
    disableField(state, "BMS denied charge");
    return;
  }

  if (config.canInput && !state.canOnline) {
    disableField(state, "CAN timeout");
    return;
  }

  if (!rpmAllowsCharge(config, state)) {
    disableField(state, "RPM holdoff");
    return;
  }

  const uint8_t voltageRequest = voltageRequestDuty(config, state, state.pwmDuty);
  const uint8_t currentCap = currentCapDuty(config, state, state.pwmDuty);
  const uint8_t tempCap = tempCapDuty(config, state, max<uint8_t>(state.pwmDuty, voltageRequest));

  state.requestedDuty = min(voltageRequest, min(currentCap, tempCap));
  state.pwmDuty = applyRateLimit(state.pwmDuty, state.requestedDuty);

  setFieldDuty(state.pwmDuty);
  state.fieldEnabled = state.pwmDuty > 0;
  state.stage = state.fieldEnabled ? "Bulk" : "Idle";
}
