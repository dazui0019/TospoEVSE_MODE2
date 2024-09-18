#include "evse_relay.h"

/**
 * @brief   继电器初始化(PB3是JTAG端口, 所以需要关闭JTAG，然后将调试功能配置成SWD模式)
 * @param   none
 * @retval  none
 * @note    RLY_CTRL --> PB3
 *          ADH Detect --> PB4(L1), PB5(N)
 */
void evse_relay_init()
{
    /* GPIO配置 */
    rcu_periph_clock_enable(RLY_PORT_RCU);
    gpio_init(RLY_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, RLY_PIN);
    evse_relay_ctrl(open);
}

/**
 * @brief   继电器控制
 * @param   state = open: 打开继电器
 *          state = close: 关闭继电器
 */
void evse_relay_ctrl(relay_state_t state)
{
    if(state == open){
        GPIO_BC(RLY_PORT) = (uint32_t)RLY_PIN;
    }else if(state == close){
        GPIO_BOP(RLY_PORT) = (uint32_t)RLY_PIN;
    }else{
        ;
    }
}
