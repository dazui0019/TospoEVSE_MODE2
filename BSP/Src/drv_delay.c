/**
 * @file    drv_delay.c
 * @note    有两种实现方式, 直接使用定时器(SysTick、TIMER5或TIMER6)或者DWT外设
 */

#include "gd32f30x.h"
#include "drv_delay.h"
#include "printf.h"
#include "gd32_hal.h"
#include "drv_timer.h"
#ifdef __RTOS
#include "FreeRTOS.h"
#include "task.h"
#endif /* __RTOS */

/* 选择时基单元，下面两个都不开的话默认是SysTick, 如果需要使用RTOS就需要开启下面定时器中的一个 */
// #define TIME_BASE_TIMER5
#define TIME_BASE_TIMER6

__IO uint32_t uwTick = 0;
TickFreqTypeDef uwTickFreq = TICK_FREQ_DEFAULT;  /* 1KHz */

/**
 * @brief      configure systick
 * @param[in]  none
 * @param[out] none
 * @retval     none
*/
void systick_config(void)
{
    /* setup systick timer for 1000Hz interrupts */
    if(SysTick_Config(SystemCoreClock / 1000U)) {
        /* capture error */
        while(1) {
        }
    }
    /* configure the systick handler priority */
    NVIC_SetPriority(SysTick_IRQn, 0x00U);
}

/* 实现类似STM32 HAL_Delay() 的延时函数 */

/**
  * @brief This function is called to increment  a global variable "uwTick"
  *        used as application time base.
  * @note In the default implementation, this variable is incremented each 1ms
  *       in SysTick ISR.
  * @note This function is declared as __weak to be overwritten in case of other 
  *      implementations in user file.
  * @retval None
  */
__attribute__((always_inline)) void incTick(void)
{
    uwTick += (uint32_t)uwTickFreq;
}

/**
  * @brief Provides a tick value in millisecond.
  * @note This function is declared as __weak to be overwritten in case of other 
  *       implementations in user file.
  * @retval tick value
  */
uint32_t getTick(void)
{
    return uwTick;
}

#ifdef __RTOS
/**
 * @brief   依赖于FreeRTOS的延时函数
 * @note    该函数需要在开启任务调度器后使用, 不需要调用delay_init()
 * @retval  none
*/
void delay_ms(uint32_t Delay)
{
    uint32_t tickstart = xTaskGetTickCount();
    uint32_t wait = Delay;

    /* Add a freq to guarantee minimum wait */
    if (wait < GD_MAX_DELAY)
    {
        wait += (uint32_t)(uwTickFreq);
    }

    while((xTaskGetTickCount() - tickstart) < wait)
    {
    }
}
#else
/**
  * @brief This function provides minimum delay (in milliseconds) based 
  *        on variable incremented.
  * @note In the default implementation , SysTick timer is the source of time base.
  *       It is used to generate interrupts at regular time intervals where uwTick
  *       is incremented.
  * @note This function is declared as __weak to be overwritten in case of other
  *       implementations in user file.
  * @param Delay specifies the delay time length, in milliseconds.
  * @retval None
  */
void delay_ms(uint32_t Delay)
{
    uint32_t tickstart = getTick();
    uint32_t wait = Delay;

    /* Add a freq to guarantee minimum wait */
    if (wait < GD_MAX_DELAY)
    {
        wait += (uint32_t)(uwTickFreq);
        while((getTick() - tickstart) < wait)
        {
        }
    }
}
#endif /* __RTOS */

/**
 * @brief      初始化延时函数, 函数内的宏定义在systick.h中
 * @param[in]  none
 * @param[out] none
 * @retval     none
*/
void delay_init(void)
{
    #if (!defined TIME_BASE_TIMER5) && (!defined TIME_BASE_TIMER6)
    systick_config();
    #endif /* (!defined TIME_BASE_TIMER5) && (!defined TIME_BASE_TIMER6) */
  
    #if (defined TIME_BASE_TIMER5) && (!defined TIME_BASE_TIMER6)
    basic_timer5_init(1000);
    #endif /* (defined TIME_BASE_TIMER5) && (!defined TIME_BASE_TIMER6) */

    #if (!defined TIME_BASE_TIMER5) && (defined TIME_BASE_TIMER6)
    basic_timer6_init(1000);
    #endif /* (defined TIME_BASE_TIMER5) && (!defined TIME_BASE_TIMER6) */
}

void delay_deinit(void)
{
    #if (!defined TIME_BASE_TIMER5) && (!defined TIME_BASE_TIMER6)
    // SysTick 应该不能随便关的
    #endif /* (!defined TIME_BASE_TIMER5) && (!defined TIME_BASE_TIMER6) */
  
    #if (defined TIME_BASE_TIMER5) && (!defined TIME_BASE_TIMER6)
    nvic_irq_disable(TIMER5_IRQn);
    rcu_periph_clock_disable(RCU_TIMER5);
    timer_deinit(TIMER5);
    #endif /* (defined TIME_BASE_TIMER5) && (!defined TIME_BASE_TIMER6) */

    #if (!defined TIME_BASE_TIMER5) && (defined TIME_BASE_TIMER6)
    nvic_irq_disable(TIMER6_IRQn);
    rcu_periph_clock_disable(RCU_TIMER6);
    timer_deinit(TIMER6);
    #endif /* (defined TIME_BASE_TIMER5) && (!defined TIME_BASE_TIMER6) */
}

#if (defined TIME_BASE_TIMER5)
void TIMER5_IRQHandler(void){
    if(SET == timer_interrupt_flag_get(TIMER5, TIMER_INT_FLAG_UP)){
        timer_interrupt_flag_clear(TIMER5, TIMER_INT_FLAG_UP);
        #ifdef TIME_BASE_TIMER5
        incTick();  // 将TIMER5作为延时函数的时基单元
        #endif /* TIME_BASE_TIMER5 */
    }
}
#endif

#if (defined TIME_BASE_TIMER6)
void TIMER6_IRQHandler(void){
    if(SET == timer_interrupt_flag_get(TIMER6, TIMER_INT_FLAG_UP)){
        adc_enable(ADC0);
        timer_interrupt_flag_clear(TIMER6, TIMER_INT_FLAG_UP);
        #ifdef TIME_BASE_TIMER6
        incTick();  // 将TIMER6作为延时函数的时基单元
        #endif /* TIME_BASE_TIMER6 */
    }
}
#endif

/* 用DWT外设实现延时 */

/**
 * @brief      初始化DWT外设(并清除计数器)
 */
void dwt_init(void)
{
    /* 使能DWT外设 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    /* DWT CYCCNT寄存器计数清0 */
    DWT->CYCCNT = (uint32_t)0u;
    /* 使能Cortex-M DWT CYCCNT寄存器 */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void dwt_delay_us(__IO uint16_t us)
{
    uint32_t ticks, t_last, t_now, tcnt = 0;
    ticks = us * (SystemCoreClock / 1000000);
    /* DWT CYCCNT寄存器计数清0 */
    dwt_init();

    t_last = (uint32_t)DWT->CYCCNT;
    while (1) {
        t_now = (uint32_t)DWT->CYCCNT;
        if (t_now != t_last) {
            /* 32位计数器是递增计数器(溢出处理) */
            if (t_now > t_last) {tcnt += t_now - t_last;}
            else {tcnt += UINT32_MAX - t_last + t_now;}

            t_last = t_now;

            /*时间超过/等于要延迟的时间,则退出 */
            if (tcnt >= ticks)break;
        }
    }
}

void dwt_delay_ms(__IO uint16_t ms)
{
    uint32_t ticks, t_last, t_now, tcnt = 0;
    ticks = ms * (SystemCoreClock / 1000);
    /* DWT CYCCNT寄存器计数清0 */
    dwt_init();

    t_last = (uint32_t)DWT->CYCCNT;
    while (1) {
        t_now = (uint32_t)DWT->CYCCNT;
        if (t_now != t_last) {
            /* 32位计数器是递增计数器(溢出处理) */
            if (t_now > t_last) {tcnt += t_now - t_last;}
            else {tcnt += UINT32_MAX - t_last + t_now;}

            t_last = t_now;

            /*时间超过/等于要延迟的时间,则退出 */
            if (tcnt >= ticks)break;
        }
    }
}
