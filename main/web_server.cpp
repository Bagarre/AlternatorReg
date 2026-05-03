#include "web_server.h"
#include "config_store.h"
#include "log_buffer.h"
#include "wifi_manager.h"
#include "control_loop.h"
#include "sensors.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <ESP.h>

String getStatusJson(const AppConfig& config, const AppState& state) {
  StaticJsonDocument<1280> doc;
  doc["voltage"] = state.busVoltage;
  if (isfinite(state.chargeCurrent)) doc["current"] = state.chargeCurrent; else doc["current"].set((const char*) nullptr);
  if (isfinite(state.shuntVoltageMV)) doc["shunt_mv"] = state.shuntVoltageMV; else doc["shunt_mv"].set((const char*) nullptr);
  doc["pwm"] = state.pwmDuty;
  doc["requested_pwm"] = state.requestedDuty;
  doc["pid_requested_pwm"] = state.pidRequestedDuty;
  doc["temp_duty_cap"] = state.tempDutyCap;
  doc["current_duty_cap"] = state.currentDutyCap;
  doc["soft_start_duty_cap"] = state.softStartDutyCap;
  doc["pid_integral"] = state.pidIntegral;
  if (state.altTempSensorOk && isfinite(state.alternatorTempC)) {
    doc["alt_temp_c"] = state.alternatorTempC;
    doc["alt_temp_f"] = state.alternatorTempC * 1.8f + 32.0f;
  } else {
    doc["alt_temp_c"].set((const char*) nullptr);
    doc["alt_temp_f"].set((const char*) nullptr);
  }
  doc["alt_temp_sensor_ok"] = state.altTempSensorOk;
  if (state.engineRoomTempSensorOk && isfinite(state.engineRoomTempC)) {
    doc["engine_room_temp_c"] = state.engineRoomTempC;
    doc["engine_room_temp_f"] = state.engineRoomTempC * 1.8f + 32.0f;
  } else {
    doc["engine_room_temp_c"].set((const char*) nullptr);
    doc["engine_room_temp_f"].set((const char*) nullptr);
  }
  doc["engine_room_temp_sensor_ok"] = state.engineRoomTempSensorOk;
  doc["rpm"] = state.currentRPM;
  doc["stage"] = state.stage;
  doc["can_status"] = state.canStatus;
  if (state.lastCANMillis == 0) doc["last_can_ms_ago"].set((const char*) nullptr);
  else doc["last_can_ms_ago"] = millis() - state.lastCANMillis;
  doc["bms_permission"] = state.bmsAllowsCharge;
  doc["enabled"] = state.systemEnabled;
  doc["field_enabled"] = state.fieldEnabled;
  doc["ina226_available"] = state.ina226Available;
  doc["startup_check_complete"] = state.startupCheckComplete;
  doc["startup_check_ok"] = state.startupCheckOk;
  doc["startup_status"] = state.startupStatus;
  doc["maintenance_mode"] = maintenanceMode();
  doc["last_disable_reason"] = state.lastDisableReason;
  doc["network_mode"] = getNetworkMode();
  doc["ip_address"] = getIPAddressText();
  doc["target_voltage"] = config.targetVoltage;
  doc["current_limit"] = config.currentLimit;
  doc["pid_kp"] = config.pidKp;
  doc["pid_ki"] = config.pidKi;
  doc["pid_kd"] = config.pidKd;
  doc["derate_start_temp_c"] = config.derateStartTempC;
  doc["derate_stop_temp_c"] = config.derateStopTempC;
  doc["soft_start_seconds"] = config.softStartSeconds;
  doc["min_start_rpm"] = config.minStartRPM;
  doc["rpm_hold_seconds"] = config.rpmHoldSeconds;

  String out;
  serializeJson(doc, out);
  return out;
}

String getConfigJson(const AppConfig& config) {
  StaticJsonDocument<768> doc;
  doc["targetVoltage"] = config.targetVoltage;
  doc["currentLimit"] = config.currentLimit;
  doc["pidKp"] = config.pidKp;
  doc["pidKi"] = config.pidKi;
  doc["pidKd"] = config.pidKd;
  doc["derateStartTemp"] = config.derateStartTempC;
  doc["derateStopTemp"] = config.derateStopTempC;
  doc["softStartSeconds"] = config.softStartSeconds;
  doc["minStartRPM"] = config.minStartRPM;
  doc["rpmHoldSeconds"] = config.rpmHoldSeconds;
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

static const char* FALLBACK_INDEX_HTML = R"rawliteral(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>SVBC Alternator Regulator</title>
  <style>
    body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;margin:0;background:#eef2f7;color:#111827}
    .wrap{max-width:720px;margin:0 auto;padding:20px}
    .card{background:white;border-radius:18px;padding:18px;margin:14px 0;box-shadow:0 8px 24px rgba(0,0,0,.08)}
    h1{font-size:24px;margin:0 0 6px}
    .muted{color:#6b7280}
    .grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}
    .val{font-size:26px;font-weight:700}
    .bad{color:#dc2626}
    .ok{color:#16a34a}
    button{font-size:18px;padding:12px 18px;border:0;border-radius:12px;background:#2563eb;color:white}
    .small{font-size:13px;color:#6b7280}
    pre{white-space:pre-wrap;word-break:break-word;background:#111827;color:#d1d5db;padding:12px;border-radius:12px}
  </style>
</head>
<body>
<div class="wrap">
  <h1>SVBC Alternator Regulator</h1>
  <div class="muted">Embedded fallback UI. LittleFS did not provide /index.html.</div>
  <div class="card"><div id="status" class="muted">Loading status...</div></div>
  <div class="card"><button onclick="toggle()" id="btn">Toggle Enabled</button></div>
  <div class="card"><div class="small">Raw /api/status</div><pre id="raw"></pre></div>
</div>
<script>
let last=null;
function fmt(v,u=''){return v===null||v===undefined||Number.isNaN(v)?'NA':Number(v).toFixed(2)+u}
async function load(){
  try{
    const r=await fetch('/api/status'); last=await r.json();
    const s=last;
    document.getElementById('status').innerHTML = `
      <div class="grid">
        <div><div class="muted">Voltage</div><div class="val">${fmt(s.voltage,' V')}</div></div>
        <div><div class="muted">Current</div><div class="val">${fmt(s.current,' A')}</div></div>
        <div><div class="muted">Alt Temp</div><div class="val">${fmt(s.alt_temp_f,' °F')}</div></div>
        <div><div class="muted">Ambient</div><div class="val">${fmt(s.engine_room_temp_f,' °F')}</div></div>
      </div>
      <p>Startup: <b class="${s.startup_check_ok?'ok':'bad'}">${s.startup_status||'unknown'}</b></p>
      <p>Field: ${s.field_enabled?'ON':'OFF'} | Enabled: ${s.enabled?'YES':'NO'} | Last reason: ${s.last_disable_reason||''}</p>`;
    document.getElementById('btn').textContent=s.enabled?'Disable':'Enable';
    document.getElementById('raw').textContent=JSON.stringify(s,null,2);
  }catch(e){document.getElementById('status').textContent='Status API failed: '+e;}
}
async function toggle(){
  if(!last) return;
  await fetch('/api/enable',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({enabled:!last.enabled})});
  load();
}
setInterval(load,1000); load();
</script>
</body>
</html>
)rawliteral";

static void sendIndex(AsyncWebServerRequest* request) {
  if (LittleFS.exists("/index.html")) {
    request->send(LittleFS, "/index.html", "text/html");
  } else {
    request->send(200, "text/html", FALLBACK_INDEX_HTML);
  }
}

void setupWebServer(AsyncWebServer& server, AppConfig& config, AppState& state) {
  const bool fsOk = LittleFS.begin(true);

  if (!fsOk) {
    Serial.println("LittleFS mount failed; using embedded fallback UI");
    addLog("LittleFS mount failed; using embedded fallback UI");
  } else {
    Serial.println("LittleFS mounted");
    Serial.print("LittleFS /index.html exists: ");
    Serial.println(LittleFS.exists("/index.html") ? "YES" : "NO");
    addLog(LittleFS.exists("/index.html") ? "LittleFS index.html found" : "LittleFS index.html missing; using fallback UI");
  }

  server.on("/api/status", HTTP_GET, [&](AsyncWebServerRequest* request) {
    request->send(200, "application/json", getStatusJson(config, state));
  });

  server.on("/api/config", HTTP_GET, [&](AsyncWebServerRequest* request) {
    request->send(200, "application/json", getConfigJson(config));
  });

  server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest* request) {}, nullptr,
    [&](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
      StaticJsonDocument<768> doc;
      if (!parseJsonBody(request, data, len, doc)) return;

      config.targetVoltage = doc["targetVoltage"] | config.targetVoltage;
      config.currentLimit  = doc["currentLimit"]  | config.currentLimit;
      config.pidKp         = doc["pidKp"]         | config.pidKp;
      config.pidKi         = doc["pidKi"]         | config.pidKi;
      config.pidKd         = doc["pidKd"]         | config.pidKd;
      config.derateStartTempC = doc["derateStartTemp"] | config.derateStartTempC;
      config.derateStopTempC  = doc["derateStopTemp"]  | config.derateStopTempC;
      config.softStartSeconds = doc["softStartSeconds"] | config.softStartSeconds;
      config.minStartRPM      = doc["minStartRPM"]      | config.minStartRPM;
      config.rpmHoldSeconds   = doc["rpmHoldSeconds"]   | config.rpmHoldSeconds;
      config.canInput         = doc["canInput"]         | config.canInput;

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
      if (!parseJsonBody(request, data, len, doc)) return;

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
      if (!parseJsonBody(request, data, len, doc)) return;

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

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendIndex(request);
  });

  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendIndex(request);
  });

  server.serveStatic("/", LittleFS, "/");

  server.onNotFound([](AsyncWebServerRequest* request) {
    request->send(404, "text/plain", "Not found. If this is a UI asset, upload the LittleFS data filesystem from the data/ folder.");
  });

  server.begin();
}