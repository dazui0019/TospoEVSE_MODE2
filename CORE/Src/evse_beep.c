#include "evse_beep.h"
#include "drv_delay.h"

void evse_beep_init(void){
    rcu_periph_clock_enable(BEEP_PIN_RCU);
    gpio_init(BEEP_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, BEEP_PIN);
    GPIO_BC(BEEP_PORT) = BEEP_PIN;
}

void evse_beep(void){
    GPIO_BOP(BEEP_PORT) = BEEP_PIN;
    delay_ms(100);
    GPIO_BC(BEEP_PORT) = BEEP_PIN;
}
