#pragma once

#include "gd32f30x.h"

#define BEEP_PIN_RCU    RCU_GPIOB
#define BEEP_PIN        GPIO_PIN_8
#define BEEP_PORT       GPIOB

void evse_beep_init();
void evse_beep();
void evse_beeeep(void);
void evse_beep_ctrl(ControlStatus status);
