#pragma once

#include "app_types.h"

void loadConfig(AppConfig& config);
void saveConfig(const AppConfig& config);
void resetNetworkConfig(AppConfig& config);
