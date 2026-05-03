#include "control_loop.h"
#include "field_driver.h"
#include "log_buffer.h"
#include <Arduino.h>
#include <math.h>

namespace {
constexpr unsigned long CONTROL_INTERVAL_MS = 100;
constexpr uint8_t DUTY_MAX = 255;
constexpr uint8_t DUTY_FALL_PER_TICK = 12;
constexpr float PID_INTEGRAL_LIMIT = 80.0f;
constexpr float CURRENT_LIMIT_DERATE_BAND_A = 25.0f;
constexpr float MIN_DERATE_SPAN_C = 2.0f;

uint8_t clampDuty(int value) {
  if (value < 0) return 0;
  if (value > DUTY_MAX) return DUTY_MAX;
  return static_cast<uint8_t>(value);
}

uint8_t applyFallLimit(uint8_t currentDuty, uint8_t requestedDuty) {
  // Upward ramp is handled by the soft-start cap. Downward movement is allowed
  // to be faster for safety, but not an instant cliff unless a hard fault occurs.
  if (requestedDuty >= currentDuty) return requestedDuty;
  return static_cast<uint8_t>(max<int>(currentDuty - DUTY_FALL_PER_TICK, requestedDuty));
}

bool rpmAllowsCharge(const AppConfig& config, AppState& state) {
  const unsigned long now = millis();

  if (!config.canInput) {
    return true;
  }

  if (state.currentRPM >= config.minStartRPM) {
    if (state.rpmAboveThresholdStartMillis == 0) {
      state.rpmAboveThresholdStartMillis = now;
    }
  } else {
    state.rpmAboveThresholdStartMillis = 0;
    state.regulationStartMillis = 0;
  }

  if (state.rpmAboveThresholdStartMillis == 0) {
    return false;
  }

  return (now - state.rpmAboveThresholdStartMillis) >= static_cast<unsigned long>(config.rpmHoldSeconds * 1000.0f);
}

uint8_t pidVoltageDuty(const AppConfig& config, AppState& state, float dtSeconds) {
  if (!isfinite(state.busVoltage) || dtSeconds <= 0.0f) {
    return 0;
  }

  const float error = config.targetVoltage - state.busVoltage;

  // Anti-windup: only integrate when we are not already badly over voltage.
  if (error > -0.15f) {
    state.pidIntegral += error * dtSeconds;
    state.pidIntegral = constrain(state.pidIntegral, -PID_INTEGRAL_LIMIT, PID_INTEGRAL_LIMIT);
  }

  const float derivative = (error - state.lastPidError) / dtSeconds;
  state.lastPidError = error;

  float output = (config.pidKp * error) +
                 (config.pidKi * state.pidIntegral) +
                 (config.pidKd * derivative);

  // Bias around current duty so the PID trims field instead of acting like a
  // bang-bang controller from zero every tick.
  output += state.pwmDuty;

  return clampDuty(static_cast<int>(roundf(output)));
}

uint8_t currentLimitCap(const AppConfig& config, const AppState& state) {
  if (!isfinite(state.chargeCurrent)) return 0;
  if (config.currentLimit <= 0.0f) return DUTY_MAX;
  if (state.chargeCurrent <= config.currentLimit) return DUTY_MAX;

  const float over = state.chargeCurrent - config.currentLimit;
  const float ratio = 1.0f - constrain(over / CURRENT_LIMIT_DERATE_BAND_A, 0.0f, 1.0f);
  return clampDuty(static_cast<int>(roundf(DUTY_MAX * ratio)));
}

uint8_t tempDerateCap(const AppConfig& config, const AppState& state) {
  if (!state.altTempSensorOk || !isfinite(state.alternatorTempC)) return 0;

  const float startC = config.derateStartTempC;
  const float stopC = max(config.derateStopTempC, startC + MIN_DERATE_SPAN_C);

  if (state.alternatorTempC <= startC) return DUTY_MAX;
  if (state.alternatorTempC >= stopC) return 0;

  const float ratio = (stopC - state.alternatorTempC) / (stopC - startC);
  return clampDuty(static_cast<int>(roundf(DUTY_MAX * constrain(ratio, 0.0f, 1.0f))));
}

void updateHardTempCutoff(const AppConfig& config, AppState& state) {
  if (!state.altTempSensorOk || !isfinite(state.alternatorTempC)) {
    state.alternatorOverTemp = false;
    return;
  }

  const float cutoffC = max(config.hardCutoffTempC, config.derateStopTempC);
  const float resetC = min(config.hardCutoffResetTempC, cutoffC - 1.0f);

  if (!state.alternatorTempCutoffLatched && state.alternatorTempC >= cutoffC) {
    state.alternatorTempCutoffLatched = true;
    addLog("Alternator hard temp cutoff tripped", state.busVoltage, state.chargeCurrent, state.alternatorTempC);
  } else if (state.alternatorTempCutoffLatched && state.alternatorTempC <= resetC) {
    state.alternatorTempCutoffLatched = false;
    addLog("Alternator hard temp cutoff cleared", state.busVoltage, state.chargeCurrent, state.alternatorTempC);
  }

  state.alternatorOverTemp = state.alternatorTempCutoffLatched;
}


void updateSocInhibit(const AppConfig& config, AppState& state) {
  if (!config.socInhibitEnabled) {
    if (state.socInhibitLatched) {
      addLog("SOC inhibit disabled", state.busVoltage, state.chargeCurrent, state.alternatorTempC);
    }
    state.socInhibitLatched = false;
    return;
  }

  if (!state.cerboSocValid || !isfinite(state.cerboSocPercent)) {
    return;
  }

  const float highPct = constrain(config.socInhibitHighPercent, 0.0f, 100.0f);
  const float lowPct = constrain(min(config.socInhibitLowPercent, highPct - 1.0f), 0.0f, 99.0f);

  if (!state.socInhibitLatched && state.cerboSocPercent >= highPct) {
    state.socInhibitLatched = true;
    addLog("SOC inhibit tripped", state.busVoltage, state.chargeCurrent, state.alternatorTempC);
  } else if (state.socInhibitLatched && state.cerboSocPercent <= lowPct) {
    state.socInhibitLatched = false;
    addLog("SOC inhibit cleared", state.busVoltage, state.chargeCurrent, state.alternatorTempC);
  }
}

uint8_t softStartCap(const AppConfig& config, AppState& state) {
  if (config.softStartSeconds <= 0.0f) return DUTY_MAX;

  const unsigned long now = millis();
  if (state.regulationStartMillis == 0) {
    state.regulationStartMillis = now;
  }

  const float elapsed = (now - state.regulationStartMillis) / 1000.0f;
  const float ratio = constrain(elapsed / config.softStartSeconds, 0.0f, 1.0f);
  return clampDuty(static_cast<int>(roundf(DUTY_MAX * ratio)));
}

void resetPid(AppState& state) {
  state.pidIntegral = 0.0f;
  state.lastPidError = 0.0f;
  state.regulationStartMillis = 0;
  state.pidRequestedDuty = 0;
  state.tempDutyCap = DUTY_MAX;
  state.hardTempDutyCap = DUTY_MAX;
  state.currentDutyCap = DUTY_MAX;
  state.softStartDutyCap = DUTY_MAX;
}
}

void initControlLoop() {
}

void disableField(AppState& state, const String& reason) {
  disableFieldOutput();
  state.pwmDuty = 0;
  state.requestedDuty = 0;
  resetPid(state);
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

  const float dtSeconds = state.lastControlMillis == 0
    ? (CONTROL_INTERVAL_MS / 1000.0f)
    : (now - state.lastControlMillis) / 1000.0f;
  state.lastControlMillis = now;

  if (!state.systemEnabled) {
    disableField(state, "System disabled");
    return;
  }

  if (!state.ina226Available) {
    disableField(state, "INA226 missing");
    return;
  }

  if (!state.altTempSensorOk) {
    state.hardTempDutyCap = 0;
    disableField(state, "Alternator temp missing");
    return;
  }

  updateHardTempCutoff(config, state);
  state.hardTempDutyCap = state.alternatorTempCutoffLatched ? 0 : DUTY_MAX;
  if (state.alternatorTempCutoffLatched) {
    disableField(state, "Alternator hard temp cutoff");
    return;
  }

  if (!state.bmsAllowsCharge) {
    disableField(state, "BMS denied charge");
    return;
  }

  updateSocInhibit(config, state);
  if (state.socInhibitLatched) {
    disableField(state, "SOC inhibit");
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

  if (!isfinite(state.busVoltage)) {
    disableField(state, "Voltage unavailable");
    return;
  }

  state.pidRequestedDuty = pidVoltageDuty(config, state, dtSeconds);
  state.currentDutyCap = currentLimitCap(config, state);
  state.tempDutyCap = tempDerateCap(config, state);
  state.softStartDutyCap = softStartCap(config, state);

  state.requestedDuty = min(state.pidRequestedDuty,
                            min(state.currentDutyCap,
                                min(state.tempDutyCap,
                                    min(state.softStartDutyCap, state.hardTempDutyCap))));

  state.pwmDuty = applyFallLimit(state.pwmDuty, state.requestedDuty);

  setFieldDuty(state.pwmDuty);
  state.fieldEnabled = state.pwmDuty > 0;

  if (state.tempDutyCap < DUTY_MAX) {
    state.stage = "Thermal derate";
  } else if (state.currentDutyCap < DUTY_MAX) {
    state.stage = "Current limit";
  } else if (state.softStartDutyCap < DUTY_MAX) {
    state.stage = "Soft start";
  } else {
    state.stage = state.fieldEnabled ? "Regulating" : "Idle";
  }
  state.lastDisableReason = "None";
}
