#pragma once

#include "app_types.h"

void initControlLoop();
void runControlLoop(const AppConfig& config, AppState& state);
void disableField(AppState& state, const String& reason);
