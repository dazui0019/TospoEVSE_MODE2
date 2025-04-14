#pragma once

#include "gd32f30x.h"
#include "stdbool.h"

// relay control
#define RLY_PORT_RCU     RCU_GPIOC
#define RLY_PORT         GPIOC
#define RLY_PIN          GPIO_PIN_3

typedef enum{
    open = RESET,
    close = SET,
}relay_state_t;

typedef struct
{
    /* data */
    __IO uint8_t inited;            // 继电器是否初始化
    __IO relay_state_t relay_state; // 继电器状态
    /* function */
    void (*init)();
    void (*ctrl)(relay_state_t state);
}relay_t;

void evse_relay_init();
void evse_relay_ctrl(relay_state_t state);