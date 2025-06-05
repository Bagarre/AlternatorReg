// regulateFieldPWM.h
#pragma once

// Initializes trend history buffers
void initTrendBuffers();

// Updates trend buffers with latest readings
void updateTrends(float tempNow, float voltNow, float ampNow);

// Computes linear trend from buffer (units per second)
float computeTrend(float history[]);

// Regulates PWM based on sensor values and trend analysis
// Returns new PWM duty cycle (0–255)
int regulateFieldPWM();
