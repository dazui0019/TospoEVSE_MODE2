#include "evse_timer.h"
#include "evse_ui.h"
#include "basic_os.h"
#include "drv_rtc.h"

#define LOG_TAG "evse.delay"
#include "elog.h"

__IO uint16_t g_evse_delay = 0; // 延时上电时间
__IO uint16_t g_evse_time = 0;  // 累计充电时间

void evse_timer_init(void)
{
    rtc_reconfiguration();
    rtc_interrupt_enable(RTC_INT_SECOND);
}

void evse_delay_setting(uint16_t delay)
{
    /* 设置延时时间 */
    evse_ui_update(UI_CMD_UPDATE_TIME, delay, NULL);
}

void evse_delay_inc(void)
{
    if((g_evse_delay += EVSE_DELAY_SETP) > EVSE_MAX_DELAY){
        g_evse_delay = 0;
    }
    // log_d("g_evse_delay: %d", g_evse_delay);
    evse_ui_update(UI_CMD_UPDATE_TIME, g_evse_delay, NULL);
}

// static void task_entry_timecnt(void *parameter)
// {
//     for(;;){
//         evse_delay_inc();
//         bos_delay_ms(1000);
//     }
// }
// bos_task_export(timecnt, task_entry_timecnt, BOS_MAX_PRIORITY, NULL);

void evse_timer_cnt_start(void)
{
    rtc_interrupt_enable(RTC_INT_SECOND);
    g_evse_time = 0;
}

__IO uint8_t second_flag = false;
void RTC_IRQHandler()
{
    if(rtc_flag_get(RTC_FLAG_SECOND) != RESET){
        rtc_flag_clear(RTC_FLAG_SECOND);
        second_flag = true;
        g_evse_time++;
    }
}
