#pragma once

#include "main.h"
#include "ws2812b_port.h"
#include "evse_charge.h"

void evse_rgb_init();
void evse_rgb_set_color(uint32_t color, uint8_t Brightness, uint8_t index);
void evse_rgb_clear(uint8_t index);
void evse_rgb_state_update(evse_state_t state);
void evse_rgb_fluid(uint32_t color, uint8_t effect);
