#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>

#include "app_types.h"
#include "config_store.h"
#include "log_buffer.h"
#include "wifi_manager.h"
#include "sensors.h"
#include "field_driver.h"
#include "can_bus.h"
#include "control_loop.h"
#include "web_server.h"

AppConfig config;
AppState state;
AsyncWebServer server(80);

namespace {
void printCheckLine(const char* label, bool ok) {
  Serial.print("Checking ");
  Serial.print(label);
  Serial.print("... ");
  Serial.println(ok ? "OK" : "ERROR / not present");
}

void runStartupCheck() {
  Serial.println();
  Serial.println("=== Alternator Regulator Startup Check ===");

  printCheckLine("INA226 voltage/current monitor", state.ina226Available);
  printCheckLine("alternator DS18B20 case temp", state.altTempSensorOk);
  printCheckLine("engine room DS18B20 ambient temp", state.engineRoomTempSensorOk);

  if (config.canInput) {
    printCheckLine("N2K/CAN input", state.canOnline);
  } else {
    Serial.println("Checking N2K/CAN input... SKIPPED - canInput=false");
  }

  Serial.println("------------------------------------------");

  const bool canOk = !config.canInput || state.canOnline;
  state.startupCheckOk = state.ina226Available && state.altTempSensorOk && state.engineRoomTempSensorOk && canOk;
  state.startupCheckComplete = true;

  if (state.startupCheckOk) {
    state.startupStatus = "OK";
    Serial.println("System check complete: OK.");
    addLog("Startup check OK", state.busVoltage, state.chargeCurrent, state.alternatorTempC);
  } else {
    state.startupStatus = "Fault";
    Serial.println("System check complete: unable to continue normal regulation.");
    addLog("Startup check failed", state.busVoltage, state.chargeCurrent, state.alternatorTempC);
  }

  if (maintenanceMode()) {
    Serial.println("Maintenance mode: ENABLED - verbose serial logging active.");
  } else {
    Serial.println("Maintenance mode: disabled.");
  }

  Serial.println("==========================================");
  Serial.println();
}

void printVerboseStatus() {
  if (!maintenanceMode()) return;

  static unsigned long lastVerboseMs = 0;
  const unsigned long now = millis();
  if (now - lastVerboseMs < 2000) return;
  lastVerboseMs = now;

  Serial.print("STATUS ");
  Serial.print("INA="); Serial.print(state.ina226Available ? "OK" : "FAIL");
  Serial.print(" CAN="); Serial.print(state.canOnline ? "OK" : "FAIL");
  Serial.print(" V="); if (isfinite(state.busVoltage)) Serial.print(state.busVoltage, 3); else Serial.print("NA");
  Serial.print(" I="); if (isfinite(state.chargeCurrent)) Serial.print(state.chargeCurrent, 2); else Serial.print("NA");
  Serial.print(" AltT="); if (state.altTempSensorOk) Serial.print(state.alternatorTempC, 1); else Serial.print("FAIL");
  Serial.print(" AmbT="); if (state.engineRoomTempSensorOk) Serial.print(state.engineRoomTempC, 1); else Serial.print("FAIL");
  Serial.print(" PWM="); Serial.print(state.pwmDuty);
  Serial.print(" Stage="); Serial.print(state.stage);
  Serial.print(" Disable="); Serial.println(state.lastDisableReason);
}
}

void setup() {
  Serial.begin(115200);
  delay(200);

  state.bootMillis = millis();
  initLogBuffer();
  addLog("Boot");
  Serial.println("Alternator Regulator boot");

  loadConfig(config);

  initSensors(state);
  sampleSensors(state);

  initFieldDriver();
  initControlLoop();

  setupWiFi(config);
  if (MDNS.begin("alternator")) {
    addLog("mDNS started");
  }

  setupWebServer(server, config, state);
  setupCAN(state);

  // Give CAN a short chance to see traffic before the one-time startup check.
  const unsigned long startWait = millis();
  while (millis() - startWait < 1500) {
    processCANMessages(config, state);
    delay(10);
  }

  runStartupCheck();
}

void loop() {
  sampleSensors(state);
  processCANMessages(config, state);
  runControlLoop(config, state);
  printVerboseStatus();
}
