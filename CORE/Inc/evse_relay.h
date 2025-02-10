#pragma once

#include "gd32f30x.h"
#include "stdbool.h"

// relay control
#define RLY_PORT_RCU     RCU_GPIOA
#define RLY_PORT         GPIOA
#define RLY_PIN          GPIO_PIN_11

// voltage control
#define VC_PORT_RCU     RCU_GPIOB
#define VC_PORT         GPIOB
#define VC_PIN          GPIO_PIN_1

typedef enum{
    open = RESET,
    close = SET,
}relay_state_t;

typedef enum{
    vol_low = 0,
    vol_high = 1,
}relay_voltage_t;

typedef struct
{
    /* data */
    __IO uint8_t inited;            // 继电器是否初始化
    __IO relay_voltage_t relay_vol;         // 继电器控制电压
    __IO relay_state_t relay_state; // 继电器状态
    /* function */
    void (*init)();
    void (*ctrl)(relay_state_t state);
    void (*vol_switch)(uint8_t vol);
}relay_t;

void evse_relay_init();
void evse_relay_ctrl(relay_state_t state);
void evse_relay_vol_switch(relay_voltage_t vol);
