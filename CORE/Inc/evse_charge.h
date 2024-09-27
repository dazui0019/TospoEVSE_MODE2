#pragma once

#include "gd32f30x.h"
#include "evse_cp.h"

/**
 * @brief   充电桩状态(记录整个系统的状态)
 */
typedef enum{
    EVSE_REBOOT = 0,      // 刚启动
    EVSE_IDLE,            // 未插枪
    EVSE_HANDLE_INSERT,
    EVSE_READY_9V,        // 9v
    EVSE_READY_6V,        // 6v
    EVSE_WAIT_S2,         // 9vPWM(等待S2闭合)
    EVSE_CHARGING,        // 6vPWM
    EVSE_PAUSE,           // 充电中刷卡/涂鸦APP关闭充电
    EVSE_DONE,            // CP电平从6vPWM切换至9v(PWM)
    EVSE_FAULT,           // 充电故障
}evse_state_t;

/**
 * @brief   充电状态机状态(仅在状态机中使用)
 * @note    状态机函数数组中的索引(蓄意这里的顺序(值)不能变)
 */
typedef enum{
    EVSE_FM_REBOOT = 0,    // 刚启动
    EVSE_FM_IDLE,          // 未插枪
    EVSE_FM_READY_9V,      // 9v
    EVSE_FM_READY_6V,      // 6v
    EVSE_FM_CHARGING,      // 6vPWM
    EVSE_FM_DONE,          // CP电平从6vPWM切换至9v(PWM)
    EVSE_FM_FAULT,         // 充电故障(CP电压不在规定范围内)
    EVSE_FM_CP_LOST,       // CP断线(6V直接变成12V)
    EVSE_FM_S2_TIMEOUT,    // S2超时未断开
    EVSE_FM_CLEAR_FAULT,   // 清除充电故障
    EVSE_FM_PAUSE,         // 充电中刷卡/涂鸦APP关闭充电
    EVSE_FM_WAIT_S2,       // 9vPWM(等待S2闭合)
}evse_fm_state_t;

/**
 * @brief   充电桩状态切换事件
 * @note    用来触发充电桩状态切换
 */
typedef enum{
    CTRL_CP_12V,
    CTRL_CP_9V,
    CTRL_CP_6V,
    CTRL_CHARGING_PROCESS_START,
    CTRL_CHARGING_PROCESS_STOP,
    CTRL_S2_TIMEOUT,
    CTRL_FAULT_DETECTED,
    CTRL_FAULT_CLEAR
}ctrl_event_t;

evse_fm_state_t evse_state_reboot(cp_event_t event);
evse_fm_state_t evse_state_idle(cp_event_t event);
evse_fm_state_t evse_state_ready_9v(cp_event_t event);
evse_fm_state_t evse_state_ready_6v(cp_event_t event);
evse_fm_state_t evse_state_fault(cp_event_t event);
evse_fm_state_t evse_state_charging(cp_event_t event);
evse_fm_state_t evse_state_cp_lost(cp_event_t event);
evse_fm_state_t evse_state_done(cp_event_t event);
