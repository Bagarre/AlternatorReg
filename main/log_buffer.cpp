#include "log_buffer.h"
#include <ArduinoJson.h>

namespace {
constexpr int LOG_SIZE = 50;
LogEntry logBuffer[LOG_SIZE];
int logIndex = 0;
bool wrapped = false;
}

void initLogBuffer() {
  logIndex = 0;
  wrapped = false;
  for (auto& entry : logBuffer) {
    entry = LogEntry{};
  }
}

void addLog(const String& event, float voltage, float current, float temp) {
  LogEntry& entry = logBuffer[logIndex];
  entry.timestamp = millis();
  entry.event = event;
  entry.voltage = voltage;
  entry.current = current;
  entry.temp = temp;

  logIndex = (logIndex + 1) % LOG_SIZE;
  if (logIndex == 0) {
    wrapped = true;
  }
}

String getLogJson() {
  StaticJsonDocument<4096> doc;
  JsonArray array = doc.to<JsonArray>();

  const int count = wrapped ? LOG_SIZE : logIndex;
  const int start = wrapped ? logIndex : 0;

  for (int i = 0; i < count; ++i) {
    const int idx = (start + i) % LOG_SIZE;
    const LogEntry& entry = logBuffer[idx];
    if (entry.event.isEmpty()) {
      continue;
    }

    JsonObject obj = array.createNestedObject();
    obj["time"] = entry.timestamp;
    obj["event"] = entry.event;
    if (!isnan(entry.voltage)) obj["voltage"] = entry.voltage;
    if (!isnan(entry.current)) obj["current"] = entry.current;
    if (!isnan(entry.temp)) obj["temp"] = entry.temp;
  }

  String out;
  serializeJson(doc, out);
  return out;
}
