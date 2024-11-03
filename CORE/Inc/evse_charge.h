#pragma once

#include "gd32f30x.h"
#include "evse_cp.h"
#include "evse_relay.h"
#include "evse_config.h"

/**
 * @brief   充电桩状态(记录整个系统的状态)
 */
typedef enum{
    EVSE_REBOOT = 0,    // 刚启动
    EVSE_IDLE,          // 空闲,未插枪&未刷卡
    EVSE_WAIT_PLUGIN,   // 等待插枪,未插枪&已刷卡
    EVSE_9V,            // 9v,已插枪&未刷卡
    EVSE_9V_PWM,        // 9vPWM,已插枪&已刷卡(等待S2闭合)
    EVSE_6V,            // 6v
    EVSE_SIM_6V,        // EV使用的是简易控制导引(12V直接进入6V)
    EVSE_CHARGING,      // 6vPWM
    EVSE_DONE,          // CP电平从6vPWM切换至9v(PWM)
    EVSE_WAIT_S2_OPEN,  // 充电中刷卡
    EVSE_STOP,          // 充电中刷卡后，汽车S2断开
    EVSE_WAIT_DELAY,    // 等待倒计时结束
    EVSE_FAULT,         // 充电故障
}evse_state_t;

/**
 * @brief   充电桩状态(记录整个系统的状态)
 */
typedef enum{
    FAULT_OVER_CURRENT = 0,
    FAULT_OVER_VOLTAGE,
    FAULT_UNDER_VOLTAGE,
    FAULT_OVER_HEAT,
    FAULT_LEAKAGE,
    FAULT_RELAY_ADH,
    FAULT_CP_LOST,
    FAULT_CP_ERROR,
    FAULT_S2_TIMEOUT,
    FAULT_PE_LOST,
    FAULT_S1_LOST,
    FAULT_UNKNOWN
}evse_fault_t;

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

/**
 * @brief   CP结构体
 */
typedef struct{
    __IO evse_state_t evse_state;       // 充电桩状态
    __IO relay_state_t relay_state;     // 继电器状态
    __IO uint8_t inited;           // 充电桩是否初始化
    cp_t* p_cp;                         // cp控制
    void (*evse_relay_ctrl)(relay_state_t state);
}evse_t;

/**
 * @brief   S1检测初始化
 */
static void s1_ck_init(void);
/**
 * @brief   检查车端二极管S1是否存在
 */
static ErrStatus evse_error_ck(void);

/**
 * @brief   错误检测
 * @todo    需要根据不同的错误，进行不同的处理
 */
static ErrStatus evse_error_ck(void);
/**
 * @brief   获取最大充电电流
 */
uint8_t evse_get_max_current(void);
/**
 * @brief   设置最大充电电流
 */
void evse_set_max_current(uint8_t index);
/**
 * @brief   获取充电桩状态
 */
evse_state_t evse_get_state(void);
/**
 * @brief   在预设的电流列表中切换最大充电电流
 */
void evse_max_current_switch(void);

evse_state_t evse_idle_handle(cp_state_t);
/**
 * @brief   12V已刷卡
 */
evse_state_t evse_wait_plugin_handle(cp_state_t);
/**
 * @brief   9V未刷卡
 */
evse_state_t evse_9v_handle(cp_state_t);
/**
 * @brief   9V已刷卡
 */
evse_state_t evse_9v_pwm_handle(cp_state_t);
/**
 * @brief   6V未刷卡
 */
evse_state_t evse_6v_handle(cp_state_t);
/**
 * @brief   从12V直接进入6V(简易控制导引)
 */
evse_state_t evse_sim_6v_handle(cp_state_t);
/**
 * @brief   充电完成(从12V进入9V)
 */
evse_state_t evse_done_handle(cp_state_t);
/**
 * @brief   充电中
 */
evse_state_t evse_charging_handle(cp_state_t);
/**
 * @brief   充电故障(统一处理错误), 独立于正常的充电状态切换
 * @note    
 */
evse_state_t evse_fault_handle(cp_state_t);
/**
 * @brief   充电桩主动停止充电(充电中刷卡)
 * @note    进入时，先断开PWM输出，等切换到9V时再断开继电器。
 * @todo:   这里好像需要检测汽车从CP6V返回到CP9V的时间
 */
evse_state_t evse_stop_handle(cp_state_t cp_state);

evse_state_t evse_wait_s2_open_handle(cp_state_t);

/**
 * @brief   等待倒计时结束
 */
evse_state_t evse_wait_delay(cp_state_t cp_state);
