#pragma once

#include "app_types.h"

void setupCAN(AppState& state);
void processCANMessages(const AppConfig& config, AppState& state);
