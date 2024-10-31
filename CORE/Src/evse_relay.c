#include "evse_relay.h"
#include "basic_os.h"
#include "stddef.h"

#define LOG_TAG "evse.relay"
#include "elog.h"

relay_t g_relay = {
    .inited = false,
    .relay_state = open,
    .relay_vol = vol_high,
    .init = evse_relay_init,
    .ctrl = evse_relay_ctrl,
    .vol_switch = evse_relay_vol_switch
};

static void task_entry_relay_test(void *parameter)
{
    
    g_relay.init();
    // bos_delay_ms(1000);
    for(;;){
        if(g_relay.relay_state == open){
            g_relay.ctrl(close);
        }else{
            g_relay.ctrl(open);
        }
        bos_delay_ms(10000);
    }
}
bos_task_export(relay_test, task_entry_relay_test, BOS_MAX_PRIORITY, NULL);

static void task_entry_relay_vol_sw(void *parameter)
{
    uint8_t cnt = 0;
    for(;;){
        if(g_relay.relay_state == close || g_relay.relay_vol == vol_high){
            if(cnt++ > 1){
                cnt = 0;
                g_relay.vol_switch(vol_low);
            }
        }
        bos_delay_ms(1000);
    }
}
bos_task_export(vol_sw, task_entry_relay_vol_sw, BOS_MAX_PRIORITY, NULL);

/**
 * @brief   继电器初始化
 * @param   none
 * @retval  none
 * @note    RLY_CTRL --> PA11
 *          RLY_VOL --> PB1
 */
void evse_relay_init()
{
    /* GPIO配置 */
    rcu_periph_clock_enable(VC_PORT_RCU);
    gpio_init(VC_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, VC_PIN);
    evse_relay_vol_switch(vol_high);

    rcu_periph_clock_enable(RLY_PORT_RCU);
    gpio_init(RLY_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, RLY_PIN);
    evse_relay_ctrl(open);
    
    g_relay.inited = true;
}

/**
 * @brief   继电器控制
 * @param   state = open: 打开继电器
 *          state = close: 关闭继电器
 */
void evse_relay_ctrl(relay_state_t state)
{
    switch (state)
    {
    case open:
        GPIO_BC(RLY_PORT) = (uint32_t)RLY_PIN; // gpio_bit_reset(RLY_PORT, RLY_PIN);
        g_relay.relay_state = open;
        break;
    case close:
        /* 设置为12V */
        GPIO_BOP(VC_PORT) = (uint32_t)VC_PIN;   // gpio_bit_set(VC_PORT, VC_PIN);
        g_relay.relay_vol = vol_high;
        /* 吸合继电器 */
        GPIO_BOP(RLY_PORT) = (uint32_t)RLY_PIN; // gpio_bit_set(RLY_PORT, RLY_PIN);
        g_relay.relay_state = close;
        break;
    default:
        break;
    }
}

/**
 * @brief   切换继电器控制电压
 * @param   vol
 */
void evse_relay_vol_switch(relay_voltage_t vol_flag)
{
    switch (vol_flag)
    {
    case vol_low:
        GPIO_BC(VC_PORT) = (uint32_t)VC_PIN;    // gpio_bit_reset(VC_PORT, VC_PIN);
        g_relay.relay_vol = vol_low;
        break;
    case vol_high:
        GPIO_BOP(VC_PORT) = (uint32_t)VC_PIN;   // gpio_bit_set(VC_PORT, VC_PIN);
        g_relay.relay_vol = vol_high;
        break;
    default:
        break;
    }
}
