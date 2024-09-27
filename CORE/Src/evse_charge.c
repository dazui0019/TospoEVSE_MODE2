#include "evse_charge.h"
#include "basic_os.h"
#include "evse_relay.h"
#include "evse_gndd.h"
#include "evse_adh.h"

#define LOG_TAG "evse.evse"
#include "elog.h"

extern __IO gndd_state_t g_gndd_state;
extern __IO adh_state_t g_adh_state;
extern __IO uint16_t (*g_p_adc2_buff)[2];
extern __IO uint16_t (*g_p_adc01_buff)[2];
extern __IO cp_event_t g_cp_event;
extern __IO cp_t g_cp;

evse_fm_state_t (*state_func[])(cp_event_t) = {
    evse_state_reboot, evse_state_idle, evse_state_ready_9v,
    evse_state_ready_6v, evse_state_charging, evse_state_done,
    evse_state_fault, evse_state_cp_lost
};

void error_handle(void)
{
    return;
}

static void task_entry_evse_main(void *parameter)
{
    uint16_t adh_l = 0, adh_n = 0;

    evse_fm_state_t evse_fm_last_state, evse_fm_state = EVSE_FM_REBOOT;
    evse_fm_last_state = evse_fm_state;
    
    evse_relay_init();
    evse_relay_ctrl(open);

    log_i("EVSE_FM_REBOOT.");

    for(;;){
        bos_delay_ms(1);
        // 单独处理Fault状态(错误优先单独处理(除了CP的错误), 没有错误后再进入正常充电桩状态切换)
            if(
                g_gndd_state == EVSE_GNDD_LOST ||
                g_cp_event == EVENT_CP_ERROR
            ){
                if(evse_fm_state != EVSE_FM_FAULT){
                    evse_relay_ctrl(open);
                evse_fm_last_state = evse_fm_state = EVSE_FM_FAULT;
            }
            error_handle();
            continue;
        }

        /* 根据状态做一些相应的处理 */
        evse_fm_state = state_func[evse_fm_state](g_cp_event);
        
        if(evse_fm_last_state == evse_fm_state)
            continue;
        
        switch (evse_fm_state)
        {
        case EVSE_FM_IDLE:
            log_i("EVSE_FM_IDLE.");
            g_cp.pwm_ctrl(DISABLE);
            evse_relay_ctrl(open);
            // idle_handle();
            break;
        case EVSE_FM_READY_9V:
            log_i("EVSE_FM_READY_9V.");
            g_cp.pwm_ctrl(ENABLE);
            // ready_9v_handle();
            break;
        case EVSE_FM_READY_6V:
            // todo: 这个状态不能直接吸合继电器, 还需要判断是否需要延时(以及粘连检测)
            log_i("EVSE_FM_READY_6V.");
            // 粘连检测
            // while(g_p_adc2_buff == NULL){}
            // adh_l = get_sin_vol(g_p_adc2_buff, 100, 2);
            // adh_n = get_sin_vol(g_p_adc2_buff, 100, 3);
            // ready_6v_handle();
            break;
        case EVSE_FM_CHARGING:
            evse_relay_ctrl(close);
            log_i("EVSE_FM_CHARGING.");
            break;
        case EVSE_FM_DONE:     // 汽车主动停止充电(S2断开)
            evse_relay_ctrl(open);
            log_i("EVSE_FM_DONE.");
            break;
        case EVSE_FM_CP_LOST:
            evse_relay_ctrl(open);
            log_i("EVSE_CP_LOST.");
            break;
        case EVSE_FM_FAULT:
            log_e("EVSE_FM_FAULT.");
            break;
        default:
            break;
        }
        evse_fm_last_state = evse_fm_state;
    }
}

bos_task_export(evse_main, task_entry_evse_main, BOS_MAX_PRIORITY, NULL);

// todo 这些状态里，FAULT判断应该放到最前面。
// todo: 状态机只做状态切换，每个状态的处理放到外面进行。
/**
 * @brief   重启状态
 * @note    转换条件: 打开枪锁，并连接车端
*/
evse_fm_state_t evse_state_reboot(cp_event_t event)
{
    switch (event)
    {
    case EVENT_CP_12V:
        return EVSE_FM_IDLE;
    case EVENT_CP_9V:
        return EVSE_FM_READY_9V;
    case EVENT_CP_6V:
        return EVSE_FM_READY_6V;
    default:
        return EVSE_FM_REBOOT;
    }
}

/**
 * @brief   充电桩空闲: 未插枪状态
 * @note    转换条件: 连接车端
*/
evse_fm_state_t evse_state_idle(cp_event_t event)
{
    switch (event)
    {
    case EVENT_CP_9V:
        return EVSE_FM_READY_9V;
    case EVENT_CP_6V:
        return EVSE_FM_READY_6V;
    default:
        return EVSE_FM_IDLE;
    }
}

/**
 * @brief   已插枪
 * @note    进入条件: CP9V，转换条件: CP6V
*/
evse_fm_state_t evse_state_ready_9v(cp_event_t event)
{
    switch (event)
    {
    case EVENT_CP_6V:
        return EVSE_FM_CHARGING;
    case EVENT_CP_12V:
        return EVSE_FM_IDLE;
    default:
        return EVSE_FM_READY_9V;
    }
}

/**
 * @brief   已插枪(S2闭合)
 * @note    进入条件: CP6V，转换条件: 刷卡
*/
evse_fm_state_t evse_state_ready_6v(cp_event_t event)
{
    switch (event)
    {
    case EVENT_CP_12V:
        return EVSE_FM_IDLE;
    case EVENT_CP_9V:
        return EVSE_FM_READY_9V;
    default:
        return EVSE_FM_READY_6V;
    }
}

/**
 * @brief   充电中
 */
evse_fm_state_t evse_state_charging(cp_event_t event)
{
    switch (event)
    {
    case EVENT_CP_12V:
        return EVSE_FM_CP_LOST;
    case EVENT_CP_9V:
        return EVSE_FM_DONE;
    default:
        return EVSE_FM_CHARGING;
    }
}

evse_fm_state_t evse_state_done(cp_event_t event)
{
    switch (event)
    {
    case EVENT_CP_12V:
        return EVSE_FM_IDLE;
    case EVENT_CP_9V:
        return EVSE_FM_DONE;
    case EVENT_CP_6V:
        return EVSE_FM_CHARGING;
    default:
        return EVSE_FM_DONE;
    }
}

evse_fm_state_t evse_state_cp_lost(cp_event_t event)
{
    switch (event)
    {
    case EVENT_CP_12V:
        return EVSE_FM_IDLE;
    case EVENT_CP_9V:
        return EVSE_FM_DONE;
    case EVENT_CP_6V:
        return EVSE_FM_CHARGING;
    default:
        return EVSE_FM_CP_LOST;
    }
}

/**
 * @brief   错误状态
 * @note    进入条件: 检测到错误，转换条件: 错误清除
*/
evse_fm_state_t evse_state_fault(cp_event_t event)
{
    switch (event)
    {
    case EVENT_CP_9V:
        return EVSE_FM_DONE;
    case EVENT_CP_6V:
        return EVSE_FM_CHARGING;
    default:
        return EVSE_FM_FAULT;
    }
}
