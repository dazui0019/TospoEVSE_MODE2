#pragma once

#include "gd32f30x.h"

//RCD_TEST   测试信号控制脚
#define  RCD_TEST_PORT        GPIOB 
#define  RCD_TEST_GPIO_PIN    GPIO_PIN_7
#define  RCD_TEST_GPIO_CLK    RCU_GPIOB

#define  RCD_TEST_OPEN()      gpio_bit_set(RCD_TEST_PORT,RCD_TEST_GPIO_PIN)      //打开RCD测试模式
#define  RCD_TEST_CLOSE()     gpio_bit_reset(RCD_TEST_PORT,RCD_TEST_GPIO_PIN)    //关闭RCD测试模式

//RCD_CAL     校准信号
#define  RCD_ZERO_PORT        GPIOB 
#define  RCD_ZERO_GPIO_PIN    GPIO_PIN_5
#define  RCD_ZERO_GPIO_CLK    RCU_GPIOB

#define  RCD_ZERO_OPEN()     gpio_bit_set(RCD_ZERO_PORT,RCD_ZERO_GPIO_PIN)        //打开RCD校准模式
#define  RCD_ZERO_CLOSE()    gpio_bit_reset(RCD_ZERO_PORT,RCD_ZERO_GPIO_PIN)      //关闭RCD校准模式

/*RCD_TRIP   RCD */
#define RCD_TRIP_PIN                   GPIO_PIN_5
#define RCD_TRIP_GPIO_PORT             GPIOB
#define RCD_TRIP_GPIO_CLK              RCU_GPIOB

#define RCD_TRIP_EXTI_LINE             EXTI_5
#define RCD_TRIP_EXTI_PORT_SOURCE      GPIO_PORT_SOURCE_GPIOB
#define RCD_TRIP_EXTI_PIN_SOURCE       GPIO_PIN_SOURCE_5
#define RCD_TRIP_EXTI_IRQn             EXTI5_9_IRQn  

void evse_rcd_init(void);       //RCD 初始化
void evse_rcd_zero(void);
uint8_t evse_rcd_test(void);    //RCD 自测试操作