#pragma once

#include "drv_timer.h"

typedef enum{
    CP_INIT = 0,
    CP_12V,
    CP_9V,
    CP_6V,
    CP_ERROR,
    CP_ERROR_CLEAR,
} cp_state_t;

/**
 * @brief   充电桩状态切换事件
 * @note    用来触发充电桩状态切换
 */
typedef enum{
    EVENT_CP_NONE,
    EVENT_CP_12V,
    EVENT_CP_9V,
    EVENT_CP_6V,
    EVENT_CP_LOST,
    EVENT_CP_ERROR,
}cp_event_t;

/**
 * @brief   PWM输出状态标志
 */
typedef enum{
    CP_PWM = 0,
    CP_HIGH
}out_state_t;

/**
 * @brief   PWM输出状态标志
 */
typedef enum{
    CK_OFF = 0,
    CK_ON
}ck_state_t;

/**
 * @brief   CP结构体
 */
typedef struct{
    // todo: 添加互斥锁(更新cp状态时可能需要上锁)
    __IO uint8_t current;                   // 记录最大输出电流
    __IO cp_state_t state;                  // 记录CP状态
    __IO out_state_t pwm_state;             // 记录PWM输出状态
    __IO uint8_t ck_state;                  // 记录电平检测是否开启
    void (*init)(uint32_t f);               // 初始化
    uint8_t (*set_cur)(uint8_t);            // 设置最大电流
    void (*pwm_ctrl)(ControlStatus status); // 控制PWM输出
    void (*ck_ctrl)(ControlStatus status);  // CP电压检测控制
}cp_t;

/**
 * @brief   CP检测初始化
*/
void cp_check_init(void);
/**
 * @brief   CP PWM 输出初始化, 默认输出低电平
 * @param   f PWM频率, 单位Hz(2 - 1000000)
 * @note    TIMERxCLK(TIMERx_CK/PSC)固定为1000 000Hz(1MHz), 通过这个算出PSC寄存器的数值
*/
void cp_pwm_init(uint32_t f);
/**
 * @brief   PWM输出控制
 * @param   status:
 *              ENABLE  开启PWM输出
 *              DISABLE 关闭PWM输出
 */
void pwm_ctrl(ControlStatus status);
/**
 * @brief   开启PWM输出
 * @note    参考timer_channel_output_mode_config()函数
*/
void cp_enable(void);
/**
 * @brief   关闭PWM输出(强制输出低电平)
 * @note    参考timer_channel_output_mode_config()函数
*/
void cp_disable(void);
/**
 * @brief      设置充电电流最大值
 * @param[in]  cur: 1 ~ 63
*/
uint8_t cp_cur_set(uint8_t cur);
/**
 * @brief   开启/关闭CP检测
*/
void ck_ctrl(ControlStatus status);
/**
 * @brief   获取CP采样电压值
 * @param   none
 * @return  滤波后的CP电压值
 * @note    滤波算法暂定为限幅平均滤波
*/
uint16_t get_voltage(__IO uint16_t pBuff[], uint16_t length);
/**
 * @brief   绝对值函数
*/
int32_t abs(int32_t x);
/**
 * @brief   快速排序算法
*/
void quickSort(uint16_t arr[], int low, int high);
