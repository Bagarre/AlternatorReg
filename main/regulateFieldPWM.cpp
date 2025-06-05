// regulateFieldPWM.cpp
// This module handles PWM regulation of the alternator field based on
// sensed voltage, current, and temperature, and their recent trends.
// Trends are calculated using a fixed-length circular buffer for each metric.
//
// Author: David Ross
// License: MIT

#include "regulateFieldPWM.h"
#include <Arduino.h>
#include <Adafruit_INA260.h>
#include "config.h"

extern Adafruit_INA260 ina260;
extern float alternatorTempC;
extern void addLog(const String& msg);
extern int pwmDuty;

#define TREND_SIZE 5
static float tempHistory[TREND_SIZE];
static float voltHistory[TREND_SIZE];
static float ampHistory[TREND_SIZE];
static int trendIndex = 0;

// Initializes the trend history buffers with zeros.
void initTrendBuffers() {
  for (int i = 0; i < TREND_SIZE; i++) {
    tempHistory[i] = 0;
    voltHistory[i] = 0;
    ampHistory[i]  = 0;
  }
  trendIndex = 0;
}

// Call once per control loop with the latest measurements.
void updateTrends(float tempNow, float voltNow, float ampNow) {
  tempHistory[trendIndex] = tempNow;
  voltHistory[trendIndex] = voltNow;
  ampHistory[trendIndex]  = ampNow;
  trendIndex = (trendIndex + 1) % TREND_SIZE;
}

// Computes the rate of change over the buffer window in units/sec.
float computeTrend(float history[]) {
  int oldest = (trendIndex + 1) % TREND_SIZE;
  float delta = history[(trendIndex - 1 + TREND_SIZE) % TREND_SIZE] - history[oldest];
  float seconds = (TREND_SIZE - 1) * 1.0;  // assuming 1 Hz update rate
  return delta / seconds;
}

// Main field PWM regulation function.
// Returns adjusted PWM value from 0–255 based on trends and limits.
int regulateFieldPWM() {
  float volts = ina260.readBusVoltage() / 1000.0;
  float amps = ina260.readCurrent() / 1000.0;
  float tempC = alternatorTempC;

  updateTrends(tempC, volts, amps);
  float tempTrend = computeTrend(tempHistory);
  float voltTrend = computeTrend(voltHistory);
  float ampTrend  = computeTrend(ampHistory);

  float tempLimit = derateTemp.toFloat();
  float voltTarget = targetVoltage.toFloat();
  float ampLimit = currentLimit.toFloat();

  // Temperature threshold enforcement
  if (tempC >= tempLimit) {
    addLog("Overtemp Derate");
    return 0;
  }

  // Reactive current limit
  if (amps > ampLimit) {
    pwmDuty = max(pwmDuty - 10, 0);
    addLog("Overcurrent");
  }

  // Reactive undervoltage (e.g. battery demand)
  if (volts < voltTarget - 0.4) {
    pwmDuty = min(pwmDuty + 5, 255);
  } else if (volts > voltTarget + 0.2) {
    pwmDuty = max(pwmDuty - 5, 0);
  }

  // Trend-aware adjustment
  if (tempTrend > 1.0) {
    pwmDuty = max(pwmDuty - 10, 0);
    addLog("Temp rising fast");
  }

  if (ampTrend > 10.0) {
    pwmDuty = max(pwmDuty - 5, 0);
    addLog("Amp rising fast");
  }

  if (voltTrend < -0.3) {
    pwmDuty = max(pwmDuty - 5, 0);
    addLog("Voltage dropping");
  }

  analogWrite(FIELD_PIN, pwmDuty);
  return pwmDuty;
}
