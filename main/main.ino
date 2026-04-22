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

void setup() {
  Serial.begin(115200);
  delay(200);

  state.bootMillis = millis();
  initLogBuffer();
  addLog("Boot");

  loadConfig(config);
  initSensors(state);
  initFieldDriver();
  initControlLoop();

  setupWiFi(config);
  if (MDNS.begin("alternator")) {
    addLog("mDNS started");
  }

  setupWebServer(server, config, state);
  setupCAN(state);
}

void loop() {
  sampleSensors(state);
  processCANMessages(config, state);
  runControlLoop(config, state);
}
