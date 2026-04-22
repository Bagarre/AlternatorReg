#pragma once

#include <Arduino.h>

struct LogEntry {
  unsigned long timestamp = 0;
  String event;
  float voltage = 0.0f;
  float current = 0.0f;
  float temp = 0.0f;
};

void initLogBuffer();
void addLog(const String& event, float voltage = NAN, float current = NAN, float temp = NAN);
String getLogJson();
