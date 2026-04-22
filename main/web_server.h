#pragma once

#include "app_types.h"
#include <ESPAsyncWebServer.h>

void setupWebServer(AsyncWebServer& server, AppConfig& config, AppState& state);
String getStatusJson(const AppConfig& config, const AppState& state);
String getConfigJson(const AppConfig& config);
