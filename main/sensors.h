#pragma once

#include "app_types.h"
#include <Adafruit_INA260.h>

extern Adafruit_INA260 ina260;

void initSensors(AppState& state);
void sampleSensors(AppState& state);
float readAlternatorTemperatureC();
