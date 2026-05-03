#pragma once

#include "app_types.h"

void initSensors(AppState& state);
void sampleSensors(AppState& state);
bool maintenanceMode();
bool validDs18b20Temp(float tempC);
