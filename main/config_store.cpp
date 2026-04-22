#include "config_store.h"
#include "config_defaults.h"
#include <Preferences.h>

namespace {
constexpr const char* PREF_NAMESPACE = "altreg";
Preferences prefs;
}

void loadConfig(AppConfig& config) {
  prefs.begin(PREF_NAMESPACE, true);
  config.targetVoltage = prefs.getString("targetVoltage", DEFAULT_TARGET_VOLTAGE).toFloat();
  config.currentLimit  = prefs.getString("currentLimit",  DEFAULT_CURRENT_LIMIT).toFloat();
  config.floatVoltage  = prefs.getString("floatVoltage",  DEFAULT_FLOAT_VOLTAGE).toFloat();
  config.derateTempC   = prefs.getString("derateTemp",    DEFAULT_DERATE_TEMP).toFloat();
  config.canInput      = prefs.getBool("canInput",        DEFAULT_CAN_INPUT);
  config.wifiSSID      = prefs.getString("wifiSSID",      DEFAULT_WIFI_SSID);
  config.wifiPassword  = prefs.getString("wifiPassword",  DEFAULT_WIFI_PASSWORD);
  prefs.end();
}

void saveConfig(const AppConfig& config) {
  prefs.begin(PREF_NAMESPACE, false);
  prefs.putString("targetVoltage", String(config.targetVoltage, 2));
  prefs.putString("currentLimit",  String(config.currentLimit, 1));
  prefs.putString("floatVoltage",  String(config.floatVoltage, 2));
  prefs.putString("derateTemp",    String(config.derateTempC, 1));
  prefs.putBool("canInput",        config.canInput);
  prefs.putString("wifiSSID",      config.wifiSSID);
  prefs.putString("wifiPassword",  config.wifiPassword);
  prefs.end();
}

void resetNetworkConfig(AppConfig& config) {
  config.wifiSSID = DEFAULT_WIFI_SSID;
  config.wifiPassword = DEFAULT_WIFI_PASSWORD;
  saveConfig(config);
}
