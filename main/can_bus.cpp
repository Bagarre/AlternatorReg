#include "can_bus.h"
#include "pins.h"
#include "log_buffer.h"
#include "field_driver.h"
#include <driver/twai.h>
#include <Arduino.h>

namespace {
constexpr uint32_t PGN_ENGINE_PARAMETERS_RAPID = 127488;
constexpr unsigned long CAN_TIMEOUT_MS = 3000;

uint32_t extractPGN(uint32_t canId) {
  const uint8_t pf = (canId >> 16) & 0xFF;
  const uint8_t ps = (canId >> 8) & 0xFF;
  const uint8_t dp = (canId >> 24) & 0x01;

  if (pf < 240) {
    return (static_cast<uint32_t>(dp) << 16) | (static_cast<uint32_t>(pf) << 8);
  }

  return (static_cast<uint32_t>(dp) << 16) |
         (static_cast<uint32_t>(pf) << 8) |
         static_cast<uint32_t>(ps);
}
}

void setupCAN(AppState& state) {
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
      static_cast<gpio_num_t>(CAN_TX_PIN),
      static_cast<gpio_num_t>(CAN_RX_PIN),
      TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK &&
      twai_start() == ESP_OK) {
    state.canStatus = "OK";
    state.canOnline = true;
    addLog("CAN online");
  } else {
    state.canStatus = "Init failed";
    state.canOnline = false;
    addLog("CAN init failed");
  }
}

void processCANMessages(const AppConfig& config, AppState& state) {
  twai_message_t message;
  while (twai_receive(&message, 0) == ESP_OK) {
    state.lastCANMillis = millis();
    state.canOnline = true;
    state.canStatus = "OK";

    if (!(message.flags & TWAI_MSG_FLAG_EXTD)) {
      continue;
    }

    const uint32_t pgn = extractPGN(message.identifier);
    if (pgn == PGN_ENGINE_PARAMETERS_RAPID && message.data_length_code >= 5) {
      const uint16_t rpmRaw = message.data[3] | (message.data[4] << 8);
      state.currentRPM = rpmRaw * 0.25f;
    }
  }

  if (config.canInput && state.lastCANMillis > 0 && millis() - state.lastCANMillis > CAN_TIMEOUT_MS) {
    state.canStatus = "Timeout";
    state.canOnline = false;
  }
}
