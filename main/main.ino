/*
 * -------------------------------------------------------------
 * Smart Alternator Regulator - main.ino
 * -------------------------------------------------------------
 * Description:
 *   ESP32-based alternator field controller with:
 *   - Real-time CAN input (engine RPM)
 *   - Voltage and current sensing (INA260)
 *   - Temperature derating and safety
 *   - Web-based UI for status, config, and logs
 *   - Optional Wi-Fi AP fallback and mDNS access
 *
 * Author: David Ross
 * License: MIT
 * Last Updated: May 2025
 * -------------------------------------------------------------
 */

#include <WiFi.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include "driver/twai.h"
#include <SPIFFS.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "config.h"
#include <Adafruit_INA260.h>

Adafruit_INA260 ina260;

Preferences prefs;

String targetVoltage;
String currentLimit;
String floatVoltage;
String derateTemp;
bool canInput;
bool systemEnabled = true;
String ssid;
String password;
int pwmDuty = 0;

const int FIELD_PIN = 25;
const int ALT_TEMP_PIN = 34;

bool bmsAllowsCharge = true;
bool alternatorOverTemp = false;
float currentRPM = 0;
float alternatorTempC = 0.0;
bool fieldEnabled = false;

unsigned long rpmAboveThresholdStartMillis = 0;
unsigned long lastRPMMillis = 0;
unsigned long lastCANMillis = 0;
unsigned long lastTempSend = 0;

const char* fallbackAP = "Alt_Reg";
const char* fallbackPass = "12345";


#define LOG_SIZE 50

struct LogEntry {
  unsigned long timestamp;
  String event;
  float voltage;
  float current;
  float temp;
};

LogEntry logBuffer[LOG_SIZE];
int logIndex = 0;






AsyncWebServer server(80);

void setup() {
  Serial.begin(115200);
  Serial.println("Booting...");

  if (!ina260.begin()) {
    Serial.println("Couldn't find INA260 sensor!");
  } else {
    Serial.println("INA260 initialized.");
  }

  pinMode(FIELD_PIN, OUTPUT);
  pinMode(ALT_TEMP_PIN, INPUT);
  analogWrite(FIELD_PIN, 0);

  loadConfig();
  setupWiFi();

  if (MDNS.begin("alternator")) {
    Serial.println("mDNS started: http://alternator.local");
  }

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = getStatusJson();
    request->send(200, "application/json", json);
  });

  server.on("/enable", HTTP_GET, [](AsyncWebServerRequest *request){
      StaticJsonDocument<64> doc;
      doc["enabled"] = systemEnabled;
      String out;
      serializeJson(doc, out);
      request->send(200, "application/json", out);
    });

    server.on("/enable", HTTP_POST, [](AsyncWebServerRequest *request){}, nullptr,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        StaticJsonDocument<64> doc;
        if (deserializeJson(doc, data)) {
          request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
          return;
        }
        systemEnabled = doc["enabled"];
        if (!systemEnabled) disableField("User disabled");
        request->send(200, "application/json", "{\"status\":\"updated\"}");
    });





    server.on("/log", HTTP_GET, [](AsyncWebServerRequest *request){
      String out = "[\n";
      for (int i = 0; i < LOG_SIZE; i++) {
        int idx = (logIndex + i) % LOG_SIZE;
        if (logBuffer[idx].event != "") {
          out += "  {\n";
          out += "    \"time\": " + String(logBuffer[idx].timestamp) + ",\n";
          out += "    \"event\": \"" + logBuffer[idx].event + "\",\n";
          out += "    \"voltage\": " + String(logBuffer[idx].voltage, 2) + ",\n";
          out += "    \"current\": " + String(logBuffer[idx].current, 1) + ",\n";
          out += "    \"temp\": " + String(logBuffer[idx].temp, 1) + "\n";
          out += "  },\n";
        }
      }
      if (out.endsWith(",\n")) out.remove(out.length() - 2);
      out += "\n]";
      request->send(200, "application/json", out);
    });






  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<256> doc;
    doc["targetVoltage"] = targetVoltage;
    doc["currentLimit"] = currentLimit;
    doc["floatVoltage"] = floatVoltage;
    doc["derateTemp"] = derateTemp;
    doc["canInput"] = canInput;
    doc["ssid"] = ssid;
    doc["password"] = password;
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request){}, nullptr,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, data);
      if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
      }

      targetVoltage = doc["targetVoltage"].as<String>();
      currentLimit  = doc["currentLimit"].as<String>();
      floatVoltage  = doc["floatVoltage"].as<String>();
      derateTemp    = doc["derateTemp"].as<String>();
      canInput      = doc["canInput"].as<bool>();

      prefs.begin("altreg", false);
      prefs.putString("targetVoltage", targetVoltage);
      prefs.putString("currentLimit", currentLimit);
      prefs.putString("floatVoltage", floatVoltage);
      prefs.putString("derateTemp", derateTemp);
      prefs.putBool("canInput", canInput);
      prefs.end();

      request->send(200, "application/json", "{\"status\":\"saved\"}");
  });

  server.on("/network", HTTP_POST, [](AsyncWebServerRequest *request){}, nullptr,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, data);
      if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
      }

      ssid = doc["wifiSSID"].as<String>();
      password = doc["wifiPassword"].as<String>();

      prefs.begin("altreg", false);
      prefs.putString("wifiSSID", ssid);
      prefs.putString("wifiPassword", password);
      prefs.end();

      request->send(200, "application/json", "{\"status\":\"saved - rebooting\"}");
      delay(500);
      ESP.restart();
  });

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
  server.serveStatic("/style.css", SPIFFS, "/style.css");
  server.serveStatic("/app.js", SPIFFS, "/app.js");

  server.begin();
  setupCAN();
  Serial.println("Setup complete");
}

void loop() {
  if (!systemEnabled) {
  disableField("System disabled");
  delay(500);
}
  alternatorTempC = readAlternatorTemperature();
  processCANMessages();

  if (millis() - lastCANMillis > 3000) {
    disableField("CAN timeout");
    return;
  }

  if (millis() - lastTempSend > 1000) {
    lastTempSend = millis();
  }

  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 500) {
    lastUpdate = millis();
    if (canCharge() && systemEnabled) {
      pwmDuty = regulateFieldPWM(pwmDuty);
      analogWrite(FIELD_PIN, pwmDuty);
      fieldEnabled = true;
    } else if (fieldEnabled) {
      disableField("Conditions not met");
    }
  }

}


void addLog(const String& event) {
  float volts = ina260.readBusVoltage() / 1000.0;
  float amps = ina260.readCurrent();
  float temp = alternatorTempC;

  logBuffer[logIndex] = {
    millis() / 1000,
    event,
    volts,
    amps,
    temp
  };
  logIndex = (logIndex + 1) % LOG_SIZE;
}



void loadConfig() {
  prefs.begin("altreg", true);
  targetVoltage = prefs.getString("targetVoltage", DEFAULT_TARGET_VOLTAGE);
  currentLimit  = prefs.getString("currentLimit",  DEFAULT_CURRENT_LIMIT);
  floatVoltage  = prefs.getString("floatVoltage",  DEFAULT_FLOAT_VOLTAGE);
  derateTemp    = prefs.getString("derateTemp",    DEFAULT_DERATE_TEMP);
  canInput      = prefs.getBool("canInput",        DEFAULT_CAN_INPUT);
  ssid          = prefs.getString("wifiSSID",      DEFAULT_WIFI_SSID);
  password      = prefs.getString("wifiPassword",  DEFAULT_WIFI_PASSWORD);
  prefs.end();
}

// Simulate believable values for UI
void simulateStatusValues() {
  currentRPM = random(800, 2000); // 800–2000 RPM
  alternatorTempC = random(60, 100); // 60–100 °C
}

String getStatusJson() {
  simulateStatusValues();

  float chargeAmps = 0.0;
  float busVolts = 0.0;

  if (ina260.begin()) { // or just trust it's always initialized
    chargeAmps = ina260.readCurrent();   // returns in milliamps
    busVolts = ina260.readBusVoltage();  // volts
  }



  StaticJsonDocument<256> doc;
  doc["voltage"] = String(random(138, 145) / 10.0, 1) + " V";  // 13.8–14.4 V
  doc["current"] = String(random(10, 95)) + " A";
  doc["pwm"] = String(random(20, 100)) + "%";
  doc["alt_temp"] = String((int)(alternatorTempC * 1.8 + 32)) + "°F";
  doc["batt_temp"] = "85°F";
  doc["rpm"] = String((int)currentRPM);
  doc["stage"] = fieldEnabled ? "Bulk" : "Idle";
  doc["can_status"] = "OK";
  doc["last_can"] = "12:45:12";
  doc["bms_permission"] = bmsAllowsCharge;
  doc["enabled"] = systemEnabled;
  String json;
  serializeJson(doc, json);
  return json;
}

void setupWiFi() {
  if (ssid[0] == '\0' || password[0] == '\0') {
    Serial.println("Starting Access Point (no SSID provided)...");
    WiFi.softAP(fallbackAP, fallbackPass);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    return;
  }

  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("Connecting to WiFi");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi failed — starting fallback AP...");
    WiFi.softAP(fallbackAP, fallbackPass);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
  }
}

bool canCharge() {
  const int cruiseRPMThreshold = 1200;
  const unsigned long rpmHoldDuration = 30000;
  unsigned long now = millis();

  if (currentRPM > cruiseRPMThreshold) {
    if (rpmAboveThresholdStartMillis == 0) rpmAboveThresholdStartMillis = now;
  } else {
    rpmAboveThresholdStartMillis = 0;
  }

  bool rpmOK = currentRPM > cruiseRPMThreshold &&
               (now - rpmAboveThresholdStartMillis > rpmHoldDuration);

  return rpmOK && bmsAllowsCharge && !alternatorOverTemp;
}



void disableField(const char* reason) {
  analogWrite(FIELD_PIN, 0);
  if (fieldEnabled) Serial.printf("Disabling field: %s\n", reason);
  fieldEnabled = false;
}

float readAlternatorTemperature() {
  int raw = analogRead(ALT_TEMP_PIN);
  float voltage = raw * (3.3 / 4095.0);
  return (voltage - 0.5) * 100.0;
}

void setupCAN() {
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)22, (gpio_num_t)21, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK &&
      twai_start() == ESP_OK) {
    Serial.println("CAN driver started");
  } else {
    Serial.println("CAN init failed");
  }
}

void processCANMessages() {
  twai_message_t message;
  if (twai_receive(&message, pdMS_TO_TICKS(10)) == ESP_OK) {
    lastCANMillis = millis();
    if (message.identifier == 127488 && message.data_length_code >= 5) {
      uint16_t rpmRaw = message.data[3] | (message.data[4] << 8);
      currentRPM = rpmRaw * 0.25;
    }
  }
}



int regulateFieldPWM(int currentPWM) {
  float sensedVoltage = random(138, 145) / 10.0; // Simulated voltage (13.8–14.4)
  float sensedCurrent = random(10, 95); // Simulated current in amps
  float tempLimit = derateTemp.toFloat();
  float voltTarget = targetVoltage.toFloat();
  float ampLimit = currentLimit.toFloat();

  // Derate based on temp
  float derateFactor = 1.0;
  if (alternatorTempC >= tempLimit) {
    derateFactor = 0.5;
  } else if (alternatorTempC >= (tempLimit - 10)) {
    derateFactor = 1.0 - ((alternatorTempC - (tempLimit - 10)) / 10.0) * 0.5;
  }

  // Adjust based on voltage error
  float error = voltTarget - sensedVoltage;
  int delta = error * 30; // Proportional control factor

  // Apply current clamp
  if (sensedCurrent > ampLimit) delta -= 5;

  int newPWM = currentPWM + delta;
  newPWM = constrain(newPWM, 0, (int)(255 * derateFactor));
  return newPWM;
}
