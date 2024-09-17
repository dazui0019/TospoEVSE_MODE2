#ifndef __GD32F303X_TIMER_H_
#define __GD32F303X_TIMER_H_

#include "gd32f30x.h"

/* 使用优先级分组4, 无子优先级(设置为0) */
#define TIMER5_IRQ_PRE_PRIORITY 5
#define TIMER6_IRQ_PRE_PRIORITY 6

/** 
  * @brief  Timer clock source structures definition  
  */  
typedef enum 
{
    APB1_TIMER,
    APB2_TIMER
} GD_TimerSourceTypeDef;

uint32_t timer_source_clock_get(uint32_t TIMERx);

/* 基本定时器初始化 */
void basic_timer5_init(uint16_t delay_us);
void basic_timer6_init(uint16_t delay_us);
/* 通用定时器 */
void timer1_init(uint32_t f);
void timer2_init(uint16_t f);
void timer3_init(uint16_t f);
void timer7_init(uint16_t f);

void timer_pwm_config(void);

#endif /* __GD32F303X_TIMER_H_ */