#include "web_server.h"
#include "config_store.h"
#include "log_buffer.h"
#include "wifi_manager.h"
#include "control_loop.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <ESP.h>

String getStatusJson(const AppConfig& config, const AppState& state) {
  StaticJsonDocument<512> doc;
  doc["voltage"] = state.busVoltage;
  doc["current"] = state.chargeCurrent;
  doc["pwm"] = state.pwmDuty;
  doc["requested_pwm"] = state.requestedDuty;
  doc["alt_temp_c"] = state.alternatorTempC;
  doc["alt_temp_f"] = state.alternatorTempC * 1.8f + 32.0f;
  if (isnan(state.engineRoomTempC)) {
    doc["engine_room_temp_c"].set((const char*) nullptr);
  } else {
    doc["engine_room_temp_c"] = state.engineRoomTempC;
  }
  doc["rpm"] = state.currentRPM;
  doc["stage"] = state.stage;
  doc["can_status"] = state.canStatus;
  if (state.lastCANMillis == 0) {
    doc["last_can_ms_ago"].set((const char*) nullptr);
  } else {
    doc["last_can_ms_ago"] = millis() - state.lastCANMillis;
  }
  doc["bms_permission"] = state.bmsAllowsCharge;
  doc["enabled"] = state.systemEnabled;
  doc["field_enabled"] = state.fieldEnabled;
  doc["last_disable_reason"] = state.lastDisableReason;
  doc["network_mode"] = getNetworkMode();
  doc["ip_address"] = getIPAddressText();
  doc["target_voltage"] = config.targetVoltage;
  doc["current_limit"] = config.currentLimit;
  String out;
  serializeJson(doc, out);
  return out;
}

String getConfigJson(const AppConfig& config) {
  StaticJsonDocument<256> doc;
  doc["targetVoltage"] = config.targetVoltage;
  doc["currentLimit"] = config.currentLimit;
  doc["floatVoltage"] = config.floatVoltage;
  doc["derateTemp"] = config.derateTempC;
  doc["canInput"] = config.canInput;
  doc["ssid"] = config.wifiSSID;
  doc["password"] = config.wifiPassword;
  String out;
  serializeJson(doc, out);
  return out;
}

static bool parseJsonBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, JsonDocument& doc) {
  const DeserializationError error = deserializeJson(doc, data, len);
  if (error) {
    request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return false;
  }
  return true;
}

void setupWebServer(AsyncWebServer& server, AppConfig& config, AppState& state) {
  SPIFFS.begin(true);

  server.on("/api/status", HTTP_GET, [&](AsyncWebServerRequest* request) {
    request->send(200, "application/json", getStatusJson(config, state));
  });

  server.on("/api/config", HTTP_GET, [&](AsyncWebServerRequest* request) {
    request->send(200, "application/json", getConfigJson(config));
  });

  server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest* request) {}, nullptr,
    [&](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
      StaticJsonDocument<512> doc;
      if (!parseJsonBody(request, data, len, doc)) {
        return;
      }

      config.targetVoltage = doc["targetVoltage"] | config.targetVoltage;
      config.currentLimit  = doc["currentLimit"]  | config.currentLimit;
      config.floatVoltage  = doc["floatVoltage"]  | config.floatVoltage;
      config.derateTempC   = doc["derateTemp"]    | config.derateTempC;
      config.canInput      = doc["canInput"]      | config.canInput;

      saveConfig(config);
      request->send(200, "application/json", "{\"status\":\"saved\"}");
    });

  server.on("/api/enable", HTTP_GET, [&](AsyncWebServerRequest* request) {
    StaticJsonDocument<64> doc;
    doc["enabled"] = state.systemEnabled;
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  server.on("/api/enable", HTTP_POST, [](AsyncWebServerRequest* request) {}, nullptr,
    [&](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
      StaticJsonDocument<128> doc;
      if (!parseJsonBody(request, data, len, doc)) {
        return;
      }

      state.systemEnabled = doc["enabled"] | state.systemEnabled;
      if (!state.systemEnabled) {
        disableField(state, "User disabled");
      }
      request->send(200, "application/json", "{\"status\":\"updated\"}");
    });

  server.on("/api/log", HTTP_GET, [&](AsyncWebServerRequest* request) {
    request->send(200, "application/json", getLogJson());
  });

  server.on("/api/network", HTTP_POST, [](AsyncWebServerRequest* request) {}, nullptr,
    [&](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
      StaticJsonDocument<256> doc;
      if (!parseJsonBody(request, data, len, doc)) {
        return;
      }

      config.wifiSSID = String(static_cast<const char*>(doc["wifiSSID"] | ""));
      config.wifiPassword = String(static_cast<const char*>(doc["wifiPassword"] | ""));
      saveConfig(config);

      request->send(200, "application/json", "{\"status\":\"saved\",\"rebooting\":true}");
      delay(400);
      ESP.restart();
    });

  server.on("/api/network/reset", HTTP_POST, [&](AsyncWebServerRequest* request) {
    resetNetworkConfig(config);
    request->send(200, "application/json", "{\"status\":\"network_reset\",\"rebooting\":true}");
    delay(400);
    ESP.restart();
  });

  server.on("/api/reboot", HTTP_POST, [&](AsyncWebServerRequest* request) {
    request->send(200, "application/json", "{\"status\":\"rebooting\"}");
    delay(400);
    ESP.restart();
  });

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  server.begin();
}
