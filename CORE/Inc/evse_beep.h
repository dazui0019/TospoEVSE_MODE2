#pragma once

#include "gd32f30x.h"

#define BEEP_PIN_RCU    RCU_GPIOC
#define BEEP_PIN        GPIO_PIN_14
#define BEEP_PORT       GPIOC

void evse_beep_init();
void evse_beep();   
