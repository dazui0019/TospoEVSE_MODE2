#pragma once

#include "drv_timer.h"

typedef enum{
    CP_INIT = 0,
    CP_12V,
    CP_9V,
    CP_6V,
    CP_ERROR,
    CP_ERROR_CLEAR,
    CC_ERROR
} cp_state_t;

/**
 * CP频率为1KHz(周期1ms)，ADC采样频率30KHz(周期1/30ms)。
 * ADC对一个周期的CP波形，采样30次数据
*/

#define SAMPLE_NUM      (10)    // 采样次数(SAMPLE_NUM个CP周期)
#define ADC_TH          (0x30)  // ADC限幅滤波阈值

/* CP输出 */
#define CP_TIMER        (TIMER2)
#define CP_TIMER_RCU    (RCU_TIMER2)
#define CP_TIMER_CH     (TIMER_CH_1)
#define CP_PORT         (GPIOB)
#define CP_PIN          (GPIO_PIN_5)
#define CP_PORT_RCU     (RCU_GPIOB)

#define CK_ADC          ADC0
#define CK_ADC_RCU      RCU_ADC0
/* CP检测 */
#define CK_CP_PORT      GPIOB
#define CK_CP_RCU       RCU_GPIOB
#define CK_CP_PIN       GPIO_PIN_1

#define CK_CP_ADC_CH    ADC_CHANNEL_9
/* 接地检测 */
#define CK_GND_PORT     GPIOA
#define CK_GND_RCU      RCU_GPIOA
#define CK_GND_PIN      GPIO_PIN_6

#define CK_GND_ADC_CH   ADC_CHANNEL_6

// 定义CP电平阈值
#define CP_12V_TH   4000
#define CP_9V_TH    2940
#define CP_6V_TH    2150
#define CP_OFFSET   100

// 定义CP电压状态
#define STATE_CP_12V    (1<<0)
#define STATE_CP_9V     (1<<1)
#define STATE_CP_6V     (1<<2)
#define STATE_CP_3V     (1<<3)
#define STATE_CP_UNK    (1<<4)  // 未知状态
#define STATE_CP_ERROR  (1<<5)

// 车端二极管检测
#define S1_CK_PORT     GPIOB
#define S1_CK_RCU      RCU_GPIOB
#define S1_CK_PIN      GPIO_PIN_15

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
void cp_init(uint32_t f);
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

/* CP状态机 */
cp_state_t cp_state_reboot(uint16_t vol);
cp_state_t cp_state_12V(uint16_t vol);
cp_state_t cp_state_9V(uint16_t vol);
cp_state_t cp_state_6V(uint16_t vol);
cp_state_t cp_state_error(uint16_t vol);
cp_state_t cp_state_err_clear(uint16_t vol);