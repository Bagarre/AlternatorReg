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
  config.pidKp         = prefs.getString("pidKp",         DEFAULT_PID_KP).toFloat();
  config.pidKi         = prefs.getString("pidKi",         DEFAULT_PID_KI).toFloat();
  config.pidKd         = prefs.getString("pidKd",         DEFAULT_PID_KD).toFloat();
  config.derateStartTempC = prefs.getString("derateStart", DEFAULT_DERATE_START_TEMP).toFloat();
  config.derateStopTempC  = prefs.getString("derateStop",  DEFAULT_DERATE_STOP_TEMP).toFloat();
  config.softStartSeconds = prefs.getString("softStart",   DEFAULT_SOFT_START_SECONDS).toFloat();
  config.minStartRPM      = prefs.getString("minStartRPM", DEFAULT_MIN_START_RPM).toFloat();
  config.rpmHoldSeconds   = prefs.getString("rpmHoldSec", DEFAULT_RPM_HOLD_SECONDS).toFloat();
  config.canInput      = prefs.getBool("canInput",        DEFAULT_CAN_INPUT);
  config.wifiSSID      = prefs.getString("wifiSSID",      DEFAULT_WIFI_SSID);
  config.wifiPassword  = prefs.getString("wifiPassword",  DEFAULT_WIFI_PASSWORD);
  prefs.end();
}

void saveConfig(const AppConfig& config) {
  prefs.begin(PREF_NAMESPACE, false);
  prefs.putString("targetVoltage", String(config.targetVoltage, 2));
  prefs.putString("currentLimit",  String(config.currentLimit, 1));
  prefs.putString("pidKp",         String(config.pidKp, 3));
  prefs.putString("pidKi",         String(config.pidKi, 3));
  prefs.putString("pidKd",         String(config.pidKd, 3));
  prefs.putString("derateStart",   String(config.derateStartTempC, 1));
  prefs.putString("derateStop",    String(config.derateStopTempC, 1));
  prefs.putString("softStart",     String(config.softStartSeconds, 1));
  prefs.putString("minStartRPM",   String(config.minStartRPM, 0));
  prefs.putString("rpmHoldSec",    String(config.rpmHoldSeconds, 0));
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
