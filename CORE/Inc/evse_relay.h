#pragma once

#include "gd32f30x.h"

#define REALY1_GPIO_PIN  GPIO_PIN_0
#define RELAY1_GPIO_PORT GPIOA

// relay control
#define RLY_PORT_RCU     RCU_GPIOA
#define RLY_PORT         GPIOA
#define RLY_PIN          GPIO_PIN_0

typedef enum{
    open = RESET,
    close = SET,
    adh_error
}relay_state_t;


void evse_relay_init();
void evse_relay_ctrl(relay_state_t state);
