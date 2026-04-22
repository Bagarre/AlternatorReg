#pragma once

#include <Arduino.h>

void initFieldDriver();
void setFieldDuty(uint8_t duty);
uint8_t getFieldDuty();
void disableFieldOutput();
