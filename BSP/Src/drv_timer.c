#include "drv_timer.h"

/**
 * @brief       计算定时器时钟源对于对应的APBx时钟的倍频系数
 * @note        定时器的CK_TIMER是不固定的, 某几个寄存器里的值不同就会发生改变
 * @param[in]   APBx(GD_TimerSourceTypeDef): 指定APB1或者APB2
 * @retval      倍频系数
*/
uint8_t timer_clk_pll_get(GD_TimerSourceTypeDef APBx_TIMER)
{
    uint8_t psc;
    if(APBx_TIMER == APB1_TIMER)        {psc = (RCU_CFG0&RCU_CFG0_APB1PSC)>>8;}    // 获取APBxPSC的值
    else if(APBx_TIMER == APB2_TIMER)   {psc = (RCU_CFG0&RCU_CFG0_APB2PSC)>>11;}
    else                                {return 0; /* ERROR: 非法参数*/ }
    if(psc < 0b100)
        return 1;
    else
        return 2;
}

/**
 * @brief       获取指定定时器的输入时钟源
 * @note        定时器的CK_TIMER是不固定的, 某几个寄存器里的值不同就会发生改变
 * @param[in]   TIMERx(x = 0...13)
 * @retval  定时器的输入时钟源频率
*/
uint32_t timer_source_clock_get(uint32_t TIMERx)
{
    switch (TIMERx){
    case TIMER1:
    case TIMER2:
    case TIMER3:
    case TIMER4:
    case TIMER5:
    case TIMER6:
    case TIMER11:
    case TIMER12:
    case TIMER13:
        return (rcu_clock_freq_get(CK_APB1)*timer_clk_pll_get(APB1_TIMER));
    case TIMER0:
    case TIMER7:
    case TIMER8:
    case TIMER9:
    case TIMER10:
        return (rcu_clock_freq_get(CK_APB2)*timer_clk_pll_get(APB2_TIMER));
    }
    return 0; // ERROR
}

/* 基本定时器 ------------------------------------------------------------------*/
/**
 * @brief      根据给定的时间初始化TIMER5
 * @param[in]  delay_us(1 ~ 65535): 更新时间的触发周期
 * @retval     倍频系数
*/
void basic_timer5_init(uint16_t delay_us)
{
    timer_parameter_struct basic_timer_pareameter;
    rcu_periph_clock_enable(RCU_TIMER5);
    timer_deinit(TIMER5);

    timer_struct_para_init(&basic_timer_pareameter);
    /* (period-1) * (prescaler-1) = CK_TIMERx*(延时时间) */
    basic_timer_pareameter.period       = (delay_us - 1);                                   // 自动重装载寄存器值
    basic_timer_pareameter.prescaler    = ((timer_source_clock_get(TIMER5)/1000000U)-1);    // 预分频系数
    
    timer_init(TIMER5, &basic_timer_pareameter);
    timer_auto_reload_shadow_enable(TIMER5);                            // 配置TIMERx_CTL0的ARSE
    timer_update_event_enable(TIMER5);                                  // 配置TIMERx_CTL0的UPDIS
    timer_single_pulse_mode_config(TIMER5, TIMER_SP_MODE_REPETITIVE);   // 配置TIMERx_CTL0的SPM(配置为连续模式)
    timer_update_source_config(TIMER5, TIMER_UPDATE_SRC_REGULAR);       // 配置TIMERx_CTL0的UPS
    nvic_irq_enable(TIMER5_IRQn, TIMER5_IRQ_PRE_PRIORITY, 0);
    timer_interrupt_enable(TIMER5, TIMER_INT_UP);                       // 配置TIMERx_DMAINTEN的UPIE
    timer_enable(TIMER5);
}

/**
 * @brief      根据给定的时间初始化TIMER6
 * @param[in]  delay_us(1 ~ 65535): 更新时间的触发周期
 * @retval     倍频系数
*/
void basic_timer6_init(uint16_t delay_us)
{
    timer_parameter_struct basic_timer_pareameter;
    rcu_periph_clock_enable(RCU_TIMER6);
    timer_deinit(TIMER6);

    timer_struct_para_init(&basic_timer_pareameter);
    /* (period-1) * (prescaler-1) = CK_TIMERx*(延时时间) */
    basic_timer_pareameter.period       = (delay_us - 1);                                   // 自动重装载寄存器值
    basic_timer_pareameter.prescaler    = ((timer_source_clock_get(TIMER6)/1000000U)-1);    // 预分频系数
    
    timer_init(TIMER6, &basic_timer_pareameter);
    timer_auto_reload_shadow_enable(TIMER6);                            // 配置TIMERx_CTL0的ARSE
    timer_update_event_enable(TIMER6);                                  // 配置TIMERx_CTL0的UPDIS
    timer_single_pulse_mode_config(TIMER6, TIMER_SP_MODE_REPETITIVE);   // 配置TIMERx_CTL0的SPM
    timer_update_source_config(TIMER6, TIMER_UPDATE_SRC_REGULAR);       // 配置TIMERx_CTL0的UPS
    timer_interrupt_enable(TIMER6, TIMER_INT_UP);                       // 配置TIMERx_DMAINTEN的UPIE
    nvic_irq_enable(TIMER6_IRQn, TIMER5_IRQ_PRE_PRIORITY, 0);

    timer_enable(TIMER6);
}

/* 通用定时器L0 ------------------------------------------------------------------*/

/**
 * @brief   timer1 初始化
 * @param   f 定时器更新频率, 单位Hz(1 - 1000000)
 * @note    TIMER1CLK(TIMER1_CK/PSC)固定为1000 000Hz(1MHz), 通过这个算出PSC寄存器的数值
*/
void timer1_init(uint32_t f)
{
    timer_parameter_struct timer_initpara;      // 定时器基本参数
    rcu_periph_clock_enable(RCU_TIMER1);

    timer_deinit(TIMER1);
    /* TIMER configuration */
    timer_initpara.prescaler         = ((timer_source_clock_get(TIMER1)/1000000U)-1); // 预分频后是 1MHz
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = (1000000U/f)-1;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(TIMER1, &timer_initpara);

    /* 开启定时器更新事件 */
    timer_update_event_enable(TIMER1);                                  // 配置TIMERx_CTL0的UPDIS
    timer_single_pulse_mode_config(TIMER1, TIMER_SP_MODE_REPETITIVE);   // 配置TIMERx_CTL0的SPM(配置为连续模式)
    timer_update_source_config(TIMER1, TIMER_UPDATE_SRC_REGULAR);       // 配置TIMERx_CTL0的UPS
    
    /* auto-reload preload enable */
    timer_auto_reload_shadow_enable(TIMER1);
    timer_enable(TIMER1);
}

/**
 * @brief   timer2 初始化
 * @param   f 定时器更新频率, 单位Hz(1 - 1000000)
 * @note    TIMER2CLK(TIMER2_CK/PSC)固定为1000 000Hz(1MHz), 通过这个算出PSC寄存器的数值
*/
void timer2_init(uint16_t f)
{
    timer_parameter_struct timer_initpara;      // 定时器基本参数

    rcu_periph_clock_enable(RCU_TIMER2);

    timer_deinit(TIMER2);
    /* TIMER configuration */
    timer_initpara.prescaler         = ((timer_source_clock_get(TIMER2)/1000000U)-1); // TIMER2CLK(TIMER2_CK/PSC) is 100KHz
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = (1000000U/f)-1;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(TIMER2,&timer_initpara);

    /* 开启定时器更新事件 */
    timer_update_event_enable(TIMER2);                                  // 配置TIMERx_CTL0的UPDIS
    timer_single_pulse_mode_config(TIMER2, TIMER_SP_MODE_REPETITIVE);   // 配置TIMERx_CTL0的SPM(配置为连续模式)
    timer_update_source_config(TIMER2, TIMER_UPDATE_SRC_REGULAR);       // 配置TIMERx_CTL0的UPS

    /* auto-reload preload enable */
    timer_auto_reload_shadow_enable(TIMER2);
    /* auto-reload preload enable */
    timer_enable(TIMER2);
}

/**
 * @brief   timer3 pwm初始化
 * @param   f 定时器更新频率, 单位Hz(2 - 1000000)
 * @note    TIMER3CLK(TIMER3_CK/PSC)固定为1000 000Hz(1MHz), 通过这个算出PSC寄存器的数值
*/
void timer3_init(uint16_t f)
{
    timer_parameter_struct timer_initpara;      // 定时器基本参数
    timer_oc_parameter_struct timer_ocintpara;  // 定时器输出设置

    rcu_periph_clock_enable(RCU_TIMER3);

    timer_deinit(TIMER3);
    /* TIMER configuration */
    timer_initpara.prescaler         = ((timer_source_clock_get(TIMER3)/1000000U)-1); // TIMER2CLK(TIMER2_CK/PSC) is 100KHz
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = (1000000U/f)-1;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(TIMER3,&timer_initpara);

    /* CH0 configuration in PWM mode0 */
    timer_ocintpara.ocpolarity  = TIMER_OC_POLARITY_HIGH;
    timer_ocintpara.outputstate = TIMER_CCX_ENABLE;
    timer_channel_output_config(TIMER3, TIMER_CH_3, &timer_ocintpara);

    timer_channel_output_pulse_value_config(TIMER3, TIMER_CH_3, 500);
    timer_channel_output_mode_config(TIMER3, TIMER_CH_3, TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(TIMER3, TIMER_CH_3, TIMER_OC_SHADOW_DISABLE);

    timer_update_event_enable(TIMER3);                                  // 配置TIMERx_CTL0的UPDIS
    timer_single_pulse_mode_config(TIMER3, TIMER_SP_MODE_REPETITIVE);   // 配置TIMERx_CTL0的SPM(配置为连续模式)
    timer_update_source_config(TIMER3, TIMER_UPDATE_SRC_REGULAR);       // 配置TIMERx_CTL0的UPS

    /* TIMER3 primary output enable */
    timer_primary_output_config(TIMER3, ENABLE);
    /* auto-reload preload enable */
    timer_auto_reload_shadow_enable(TIMER3); 

    /* auto-reload preload enable */
    timer_enable(TIMER3);
}

void timer7_init(uint16_t f){
    timer_parameter_struct timer_initpara;      // 定时器基本参数
    timer_oc_parameter_struct timer_ocintpara;  // 定时器输出设置

    /*Configure PIN as alternate function*/
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_AF);
    gpio_init(GPIOC, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_6);

    rcu_periph_clock_enable(RCU_TIMER7);
    timer_deinit(TIMER7);

    /* TIMER configuration */
    timer_initpara.prescaler         = ((timer_source_clock_get(TIMER7)/1000000U)-1); // TIMER2CLK(TIMER2_CK/PSC) is 100KHz
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = (1000000U/f)-1;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(TIMER7, &timer_initpara);

    /* CH0 configuration in PWM mode */
    timer_ocintpara.outputstate  = TIMER_CCX_ENABLE;
    timer_ocintpara.outputnstate = TIMER_CCXN_DISABLE;
    timer_ocintpara.ocpolarity   = TIMER_OC_POLARITY_HIGH;
    timer_ocintpara.ocnpolarity  = TIMER_OCN_POLARITY_HIGH;
    timer_ocintpara.ocidlestate  = TIMER_OC_IDLE_STATE_LOW;
    timer_ocintpara.ocnidlestate = TIMER_OCN_IDLE_STATE_LOW;
    timer_channel_output_config(TIMER7,TIMER_CH_2,&timer_ocintpara);

    /* TIMERx channelx duty cycle = (((TIMER_CAR(CP_TIMER)+1)/20)/ TIMER_CAR(CP_TIMER))* 100  = 5% */    
    timer_channel_output_pulse_value_config(TIMER7, TIMER_CH_2, 950);
    timer_channel_output_mode_config(TIMER7, TIMER_CH_2, TIMER_OC_MODE_PWM0);        // 先输出低电平
    timer_channel_output_shadow_config(TIMER7, TIMER_CH_2, TIMER_OC_SHADOW_ENABLE);  // 使能CHxCV寄存器的影子寄存器
    
    timer_update_event_enable(TIMER7);                                  // 配置TIMERx_CTL0的UPDIS
    timer_single_pulse_mode_config(TIMER7, TIMER_SP_MODE_REPETITIVE);   // 配置TIMERx_CTL0的SPM(配置为连续模式)
    timer_update_source_config(TIMER7, TIMER_UPDATE_SRC_GLOBAL);        // 配置TIMERx_CTL0的UPS
    
    timer_primary_output_config(TIMER7, ENABLE);
    /* auto-reload preload enable */
    timer_auto_reload_shadow_enable(TIMER7);
    /* auto-reload preload enable */
    timer_enable(TIMER7);
}

/* PWM输出 ------------------------------------------------------------------*/
/**
 * @brief   Timer0 PWM 测试函数
 * @param   f PWM频率, 单位Hz(2 - 1000000)
 * @note    TIMER2CLK(TIMER2_CK/PSC)固定为1000 000Hz(1MHz), 通过这个算出PSC寄存器的数值
*/
void timer_pwm_config(void)
{
    /* TIMER0 configuration: generate PWM signals with different duty cycles:
       TIMER0CLK = SystemCoreClock / 120 = 1MHz */
    timer_oc_parameter_struct timer_ocintpara;
    timer_parameter_struct timer_initpara;

    /*Configure PIN as alternate function*/
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_AF);
    // gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE);
    // gpio_pin_remap_config(GPIO_TIMER1_PARTIAL_REMAP0, ENABLE);
    // gpio_init(CP_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, CP_PIN);
    gpio_pin_remap_config(GPIO_TIMER2_FULL_REMAP, ENABLE);
    gpio_init(GPIOC, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_6);

    rcu_periph_clock_enable(RCU_TIMER2);
    timer_deinit(TIMER2);

    /* TIMER0 configuration */
    timer_initpara.prescaler         = 1199;
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = 9999;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(TIMER2, &timer_initpara);

    /* CH0 configuration in PWM mode0 */
    timer_ocintpara.ocpolarity  = TIMER_OC_POLARITY_HIGH;
    timer_ocintpara.outputstate = TIMER_CCX_ENABLE;
    timer_channel_output_config(TIMER2, TIMER_CH_0, &timer_ocintpara);

    timer_channel_output_pulse_value_config(TIMER2, TIMER_CH_0, 500);
    timer_channel_output_mode_config(TIMER2, TIMER_CH_0, TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(TIMER2, TIMER_CH_0, TIMER_OC_SHADOW_DISABLE);

    /* TIMER0 primary output enable */
    timer_primary_output_config(TIMER2, ENABLE);
    /* auto-reload preload enable */
    timer_auto_reload_shadow_enable(TIMER2); 
    
    /* enable TIMER0 */
    timer_enable(TIMER2);
}
