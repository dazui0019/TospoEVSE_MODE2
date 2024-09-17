#ifndef __DELAY_H
#define __DELAY_H

#include "main.h"

/* 选择时基单元，下面两个都不开的话默认是SysTick, 如果需要使用RTOS就需要开启下面定时器中的一个 */
// #define TIME_BASE_TIMER5
// #define TIME_BASE_TIMER6

/** @defgroup HAL_TICK_FREQ Tick Frequency
  * @{
  */
typedef enum
{
  TICK_FREQ_10HZ         = 100U,
  TICK_FREQ_100HZ        = 10U,
  TICK_FREQ_1KHZ         = 1U,
  TICK_FREQ_DEFAULT      = TICK_FREQ_1KHZ
} TickFreqTypeDef;

/* configure systick */
void systick_config(void);
/* initialization delay */
void delay_init(void);

void incTick(void);
/* delay a time in milliseconds */
void delay_ms(uint32_t Delay);

uint32_t getTick(void);

/**
 * @brief      初始化DWT外设(并清除计数器)
 */
void dwt_init(void);
/**
 * @brief      用dwt外设实现的微秒级延时函数
 * @param[in]  us: 延时时间(建议小于1ms)
 * @param[out] none
 * @retval     none
 */
void dwt_delay_us(__IO uint16_t us);
/**
 * @brief      用dwt外设实现的毫秒级延时函数
 * @param[in]  us: 延时时间(建议小于1s)
 * @param[out] none
 * @retval     none
 */
void dwt_delay_ms(__IO uint16_t ms);
#endif /* SYS_TICK_H */
