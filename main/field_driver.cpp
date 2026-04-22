#include "field_driver.h"
#include "pins.h"
#include <Arduino.h>

namespace {
uint8_t currentDuty = 0;
}

void initFieldDriver() {
  pinMode(FIELD_PIN, OUTPUT);
  ledcAttachChannel(FIELD_PIN, PWM_FREQ_HZ, PWM_RESOLUTION_BITS, PWM_CHANNEL);
  ledcWriteChannel(PWM_CHANNEL, 0);
}

void setFieldDuty(uint8_t duty) {
  currentDuty = duty;
  ledcWriteChannel(PWM_CHANNEL, currentDuty);
}

uint8_t getFieldDuty() {
  return currentDuty;
}

void disableFieldOutput() {
  setFieldDuty(0);
}
