#pragma once

#include "drv_timer.h"

typedef enum{
    CP_INIT = 0,
    CP_12V,
    CP_9V,
    CP_6V,
    CP_ERROR,
} cp_state_t;

/**
 * @brief   CP结构体
 */
typedef struct{
    // todo: 添加互斥锁(更新cp状态时可能需要上锁)
    __IO uint8_t current;                   // 记录最大输出电流
    __IO cp_state_t state;                  // 记录CP状态
    __IO ControlStatus pwm_state;           // 记录PWM输出状态
    __IO ControlStatus ck_state;            // 记录电平检测是否开启
    void (*init)(uint32_t f);               // 初始化
    uint8_t (*set_cur)(uint8_t);            // 设置最大电流
    void (*pwm_ctrl)(ControlStatus status); // 控制PWM输出
    void (*ck_ctrl)(ControlStatus status);  // CP电压检测控制
    uint16_t (*get_cp_vol)(__IO uint16_t pBuff[], uint16_t length);
    cp_state_t (*get_cp_state)(float vol);
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

cp_state_t get_cp_state(float vol);

/**
 * @brief   s1二极管检测
 * @return  s1状态
 * @retval  1: 二极管缺失
 *          0: 二极管存在
 */
int evse_s1_ck(void);
