#pragma once

constexpr const char* DEFAULT_TARGET_VOLTAGE      = "14.4";
constexpr const char* DEFAULT_CURRENT_LIMIT       = "100";
constexpr const char* DEFAULT_PID_KP              = "45";
constexpr const char* DEFAULT_PID_KI              = "4";
constexpr const char* DEFAULT_PID_KD              = "0";
constexpr const char* DEFAULT_DERATE_START_TEMP   = "82";
constexpr const char* DEFAULT_DERATE_STOP_TEMP    = "96";
constexpr const char* DEFAULT_HARD_CUTOFF_TEMP     = "105";
constexpr const char* DEFAULT_HARD_CUTOFF_RESET    = "95";
constexpr const char* DEFAULT_SOFT_START_SECONDS  = "60";
constexpr const char* DEFAULT_MIN_START_RPM       = "2200";
constexpr const char* DEFAULT_RPM_HOLD_SECONDS    = "300";
constexpr const char* DEFAULT_SOC_INHIBIT_HIGH    = "95";
constexpr const char* DEFAULT_SOC_INHIBIT_LOW     = "90";
constexpr bool        DEFAULT_SOC_INHIBIT_ENABLED = true;
constexpr bool        DEFAULT_CAN_INPUT           = true;

constexpr const char* DEFAULT_WIFI_SSID      = "";
constexpr const char* DEFAULT_WIFI_PASSWORD  = "";

constexpr const char* FALLBACK_AP_SSID       = "SVBC_Alternator";
constexpr const char* FALLBACK_AP_PASSWORD   = "Nitt4agm2";
