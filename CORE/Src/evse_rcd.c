#include "evse_rcd.h"
#include "basic_os.h"
#include "EventRecorder.h"
#include "drv_delay.h"
#include "evse_relay.h"
#include "drv_timer.h"

#define LOG_TAG "evse.rcd"
#include "elog.h"

/**
 * @brief   定时器初始化
 * @param   f 定时器更新频率, 单位Hz(1 - 1000000)
 * @note    TIMER2CLK(TIMER2_CK/PSC)固定为1000 000Hz(1MHz), 通过这个算出PSC寄存器的数值
*/
void evse_rcd_timer_init(uint16_t f)
{
    timer_parameter_struct timer_initpara;      // 定时器基本参数

    rcu_periph_clock_enable(RCU_TIMER3);

    timer_deinit(TIMER3);
    /* TIMER configuration */
    timer_initpara.prescaler         = ((timer_source_clock_get(TIMER3)/1000000U)-1); // TIMER3CLK(TIMER3_CK/PSC) is 100KHz
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = (1000000U/f)-1;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(TIMER3,&timer_initpara);

    /* 开启定时器更新事件 */
    timer_update_event_enable(TIMER3);                                  // 配置TIMERx_CTL0的UPDIS
    timer_single_pulse_mode_config(TIMER3, TIMER_SP_MODE_REPETITIVE);   // 配置TIMERx_CTL0的SPM(配置为连续模式)
    timer_update_source_config(TIMER3, TIMER_UPDATE_SRC_REGULAR);       // 配置TIMERx_CTL0的UPS

    /* auto-reload preload enable */
    timer_auto_reload_shadow_enable(TIMER3);
    /* auto-reload preload enable */
    timer_disable(TIMER3);

    timer_interrupt_flag_clear(TIMER3, TIMER_INT_FLAG_UP);
    timer_interrupt_enable(TIMER3, TIMER_INT_UP);
    nvic_irq_enable(TIMER3_IRQn, 0U, 0U);
}

/**
 * @brief   RCD引脚初始化(PB3是JTAG端口, 所以需要关闭JTAG，然后将调试功能配置成SWD模式)
 * @param   none
 * @retval  none
 * @note    
 */
void evse_rcd_port_init()
{
    //RCD测试脚位-输出初始化
    rcu_periph_clock_enable(RCD_TEST_GPIO_CLK);
    gpio_init(RCD_TEST_PORT,GPIO_MODE_OUT_PP,GPIO_OSPEED_50MHZ,RCD_TEST_GPIO_PIN);

    //RCD校零脚位-输出初始化
    rcu_periph_clock_enable(RCD_ZERO_GPIO_CLK);
    gpio_init(RCD_ZERO_PORT,GPIO_MODE_OUT_PP,GPIO_OSPEED_50MHZ,RCD_ZERO_GPIO_PIN);

    //RCD TRIP初始化为IO输入模式
    rcu_periph_clock_enable(RCD_TRIP_GPIO_CLK);
    gpio_init(RCD_TRIP_GPIO_PORT,GPIO_MODE_IN_FLOATING,GPIO_OSPEED_50MHZ,RCD_TRIP_PIN);

    RCD_TEST_CLOSE();   //关闭RCD测试模式
    RCD_ZERO_CLOSE();   //关闭RCD校准模式
    delay_ms(100);  //T1 等待100MS
}

/**
 * @brief   RCD 中断触发脚初始化为中断模式
 * @param   none
 * @retval  none
 * @note    
 */
void evse_rcd_trip_init(void)
{
    /* enable the key clock */
    rcu_periph_clock_enable(RCD_TRIP_GPIO_CLK);
    rcu_periph_clock_enable(RCU_AF);

    /* configure button pin as input */
    gpio_init(RCD_TRIP_GPIO_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ,RCD_TRIP_PIN);
    /* enable and set key EXTI interrupt to the lowest priority */
    nvic_irq_enable(RCD_TRIP_EXTI_IRQn, 0U/*2U*/, 0U);

    /* connect key EXTI line to key GPIO pin */
    gpio_exti_source_select(RCD_TRIP_EXTI_PORT_SOURCE,RCD_TRIP_EXTI_PIN_SOURCE);

    exti_interrupt_flag_clear(RCD_TRIP_EXTI_LINE);
    /* configure key EXTI line */
    exti_init(RCD_TRIP_EXTI_LINE, EXTI_INTERRUPT, EXTI_TRIG_RISING);  //上升边沿触发引脚外部中断
}


/**
 * @brief   RCD 初始化(PB3是JTAG端口, 所以需要关闭JTAG，然后将调试功能配置成SWD模式)
 * @param   none
 * @retval  none
 * @note    
 */
void evse_rcd_init()
{
    evse_rcd_port_init();   //初始化RCD控制引脚
    evse_rcd_zero();        //校零操作
    evse_rcd_trip_init();   //配置为中断模式
    evse_rcd_timer_init(5); // 过滤掉5ms以内的脉冲干扰信号
}

/**
 * @brief   RCD传感器校零操作
 * @param   none
 * @retval  none
 * @note   
 */
void evse_rcd_zero(void)
{
    RCD_TEST_CLOSE();   // 关闭RCD测试模式
    RCD_ZERO_CLOSE();   // 关闭校零
    delay_ms(20);       // 等待20MS

    RCD_ZERO_OPEN();    // 开启校零
    delay_ms(80);       // T2 等待80MS
    RCD_ZERO_CLOSE();   // 关闭校零
    delay_ms(550);      // T3 等待550MS
}


/**
 * @brief   RCD传感器自检测测试操作
 * @param   none
 * @retval  RESET:测试失败  SET：测试成功
 */
uint8_t evse_rcd_test(void)
{
    uint8_t  i;
    uint8_t  test_state = SET;

    exti_interrupt_disable(RCD_TRIP_EXTI_LINE);

    RCD_ZERO_CLOSE();   //关闭校零
    RCD_TEST_OPEN();    //启动测试

    delay_ms(250);      //等待250MS

    for(i=0;i<10;i++){   
        if(gpio_input_bit_get(RCD_TRIP_GPIO_PORT,RCD_TRIP_PIN) == RESET){
            test_state = RESET;  //测试失败
            break;
        }
        delay_ms(15);           //等待25MS
    }
    RCD_TEST_CLOSE();           //停止测试

    //测试通过
    if(test_state == SET){
        delay_ms(200); //等待150MS
        //等待TRIP信号变低电平
        if(gpio_input_bit_get(RCD_TRIP_GPIO_PORT,RCD_TRIP_PIN) == SET){
            test_state = RESET;
        }
    }
    exti_flag_clear(RCD_TRIP_EXTI_LINE);
    exti_interrupt_flag_clear(RCD_TRIP_EXTI_LINE);
    exti_interrupt_enable(RCD_TRIP_EXTI_LINE);
    return test_state;
}

extern relay_t g_relay;
__IO uint8_t rcd_error_flag = false;    // rcd自检失败
void EXTI5_9_IRQHandler(void)
{
    if(RESET != exti_interrupt_flag_get(RCD_TRIP_EXTI_LINE)){
        if(gpio_input_bit_get(RCD_TRIP_GPIO_PORT,RCD_TRIP_PIN) == SET){
            // 开启定时器
            timer_interrupt_flag_clear(TIMER3, TIMER_INT_FLAG_UP);
            timer_counter_value_config(TIMER3, 0);
            timer_enable(TIMER3);
        }
        exti_interrupt_flag_clear(RCD_TRIP_EXTI_LINE);
    }
}

void TIMER3_IRQHandler(void)
{
    timer_disable(TIMER3);
    if(gpio_input_bit_get(RCD_TRIP_GPIO_PORT, RCD_TRIP_PIN) == SET){
        g_relay.ctrl(open);
        rcd_error_flag = true;
    }
    timer_interrupt_flag_clear(TIMER3, TIMER_INT_FLAG_UP);
}
