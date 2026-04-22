#include "wifi_manager.h"
#include "config_defaults.h"
#include <WiFi.h>

namespace {
String networkMode = "unknown";
String ipAddressText = "0.0.0.0";
}

void setupWiFi(const AppConfig& config) {
  if (config.wifiSSID.isEmpty()) {
    WiFi.softAP(FALLBACK_AP_SSID, FALLBACK_AP_PASSWORD);
    networkMode = "ap";
    ipAddressText = WiFi.softAPIP().toString();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(config.wifiSSID.c_str(), config.wifiPassword.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    networkMode = "station";
    ipAddressText = WiFi.localIP().toString();
    return;
  }

  WiFi.disconnect(true, true);
  WiFi.softAP(FALLBACK_AP_SSID, FALLBACK_AP_PASSWORD);
  networkMode = "ap-fallback";
  ipAddressText = WiFi.softAPIP().toString();
}

String getNetworkMode() {
  return networkMode;
}

String getIPAddressText() {
  return ipAddressText;
}
