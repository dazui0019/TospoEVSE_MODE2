#include "evse_delay.h"
#include "evse_ui.h"

#define LOG_TAG "evse.delay"
#include "elog.h"

__IO uint16_t g_evse_delay = 0;

void evse_delay_setting(uint16_t delay)
{
    /* 设置延时时间 */
}

void evse_delay_inc(void)
{
    if((g_evse_delay += EVSE_DELAY_SETP) > EVSE_MAX_DELAY){
        g_evse_delay = 0;
    }
    // log_d("g_evse_delay: %d", g_evse_delay);
    evse_ui_update(UI_CMD_UPDATE_TIME, g_evse_delay, NULL);
}
