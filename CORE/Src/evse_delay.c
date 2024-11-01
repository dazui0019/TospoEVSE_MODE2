#include "evse_delay.h"
#include "evse_ui.h"

#define LOG_TAG "evse.delay"
#include "elog.h"

static uint16_t evse_delay = 0;

void evse_delay_setting(uint16_t delay)
{
    /* 设置延时时间 */
}

void evse_delay_inc(void)
{
    uint16_t delay_temp;
    if((evse_delay += EVSE_DELAY_SETP) > EVSE_MAX_DELAY){
        evse_delay = 0;
    }
    // log_d("evse_delay: %d", evse_delay);
    delay_temp = evse_delay/60;
    delay_temp = (delay_temp<<8) | evse_delay%60;
    log_d("evse_delay: 0x%04X", delay_temp);
    evse_ui_update(UI_CMD_UPDATE_DELAY, delay_temp, NULL);
}
