#include "evse_beep.h"
#include "basic_os.h"
#include "drv_delay.h"
#include "evse_charge.h"
#include <stddef.h>

static __IO uint8_t beep_cnt = 0;
static __IO uint8_t beep_flag = false;
static void task_entry_beep(void *parameter)
{
    for(;;){
        bos_delay_ms(100);
        if(evse_get_state() != EVSE_FAULT)
            continue;

        evse_beeeep();
        bos_delay_ms(200);
    }
}
// bos_task_export(fault_beep, task_entry_beep, BOS_MAX_PRIORITY, NULL);

void evse_beep_init(void){
    rcu_periph_clock_enable(RCU_AF);
    gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE);
    
    rcu_periph_clock_enable(BEEP_PIN_RCU);
    gpio_init(BEEP_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, BEEP_PIN);
    GPIO_BC(BEEP_PORT) = BEEP_PIN;
}

void evse_beep(void){
    GPIO_BOP(BEEP_PORT) = BEEP_PIN;
    bos_delay_ms(50);
    GPIO_BC(BEEP_PORT) = BEEP_PIN;
}

void evse_beeeep(void){
    GPIO_BOP(BEEP_PORT) = BEEP_PIN;
    bos_delay_ms(200);
    GPIO_BC(BEEP_PORT) = BEEP_PIN;
}

void evse_beep_ctrl(ControlStatus status){
    if(status == ENABLE){
        GPIO_BOP(BEEP_PORT) = BEEP_PIN;
    }else if (status == DISABLE){
        GPIO_BC(BEEP_PORT) = BEEP_PIN;
    }
}
