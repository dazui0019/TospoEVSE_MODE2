#include "evse_rcd.h"
#include "basic_os.h"
#include "EventRecorder.h"
#include "drv_delay.h"
#include "evse_relay.h"

#define LOG_TAG "evse.rcd"
#include "elog.h"

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
        
    //RCD校有效值脚位-输出初始化
    rcu_periph_clock_enable(RCD_RMS_GPIO_CLK);
    gpio_init(RCD_RMS_PORT,GPIO_MODE_OUT_PP,GPIO_OSPEED_50MHZ,RCD_RMS_GPIO_PIN);
        
    //RCD TRIP初始化为IO输入模式
    rcu_periph_clock_enable(RCD_TRIP_GPIO_CLK);
    gpio_init(RCD_TRIP_GPIO_PORT,GPIO_MODE_IN_FLOATING,GPIO_OSPEED_50MHZ,RCD_TRIP_PIN);
        
    RCD_TEST_CLOSE();   //关闭RCD测试模式
    RCD_ZERO_CLOSE();   //关闭RCD校准模式
    RCD_RMS_CLOSE();    //关闭RCD校有效值模式
    bos_delay_ms(100);      //T1 等待100MS
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
}

/**
 * @brief   RCD传感器校零操作
 * @param   none
 * @retval  none
 * @note   
 */
void evse_rcd_zero(void)
{
    RCD_TEST_CLOSE();   //关闭RCD测试模式
    RCD_RMS_CLOSE();    //关闭RCD校有效值模式
    RCD_ZERO_CLOSE();   //关闭校零
    bos_delay_ms(20);   //等待20MS

    RCD_ZERO_OPEN();    //开启校零
    bos_delay_ms(80);   //T2  等待80MS
    RCD_ZERO_CLOSE();   //关闭校零
    bos_delay_ms(550);  //T3  等待550MS
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

    RCD_ZERO_CLOSE();      //关闭校零
    RCD_TEST_OPEN();       //启动测试
    bos_delay_ms(250);     //等待250MS

    for(i=0;i<10;i++){   
        if(gpio_input_bit_get(RCD_TRIP_GPIO_PORT,RCD_TRIP_PIN) == RESET){
            test_state = RESET;  //测试失败
            break;
        }
        bos_delay_ms(15);//等待25MS
    }
    RCD_TEST_CLOSE();  //停止测试

    //测试通过
    if(test_state == SET){
        bos_delay_ms(200); //等待150MS
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
void EXTI10_15_IRQHandler(void)
{
    if(RESET != exti_interrupt_flag_get(EXTI_13)){
        g_relay.ctrl(open);
        // GPIO_BC(RLY_PORT) = (uint32_t)RLY_PIN; // gpio_bit_reset(RLY_PORT, RLY_PIN);
        // g_relay.relay_state = open;
        exti_interrupt_flag_clear(EXTI_13);
    }
}
