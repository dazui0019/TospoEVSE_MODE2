#include "evse_adc.h"
#include "basic_os.h"
#include "drv_timer.h"
#include "printf.h"
#include "evse_cp.h"

#define LOG_TAG "evse.adc"
#include "elog.h"

/* 板载NTC */
#define ON_BOARD_NTC_PORT       GPIOB
#define ON_BOARD_NTC_PORT_RCU   RCU_GPIOB
#define ON_BOARD_NTC_PIN        GPIO_PIN_0

#define ON_BOARD_NTC_ADC_CH     ADC_CHANNEL_8
/* 电源插头NTC */
#define PLUG_NTC_PORT           GPIOA
#define PLUG_NTC_PORT_RCU       RCU_GPIOA
#define PLUG_NTC_PIN            GPIO_PIN_1

#define PLUG_NTC_ADC_CH         ADC_CHANNEL_1

/* 电流 */
#define CURRENT_PORT            GPIOA
#define CURRENT_PORT_RCU        RCU_GPIOA
#define CURRENT_PIN             GPIO_PIN_3

#define CURRENT_ADC_CH          ADC_CHANNEL_3
/* 火线进线电压 */
#define VL_IN_PORT      GPIOA
#define VL_IN_PORT_RCU  RCU_GPIOA
#define VL_IN_PIN       GPIO_PIN_4

#define VL_IN_ADC_CH    ADC_CHANNEL_4
/* 火线出线电压 */
#define VL_OUT_PORT     GPIOA
#define VL_OUT_PORT_RCU RCU_GPIOA
#define VL_OUT_PIN      GPIO_PIN_5

#define VL_OUT_ADC_CH   ADC_CHANNEL_5
/* 零线出线电压 */
#define VN_IN_PORT      GPIOA
#define VN_IN_PORT_RCU  RCU_GPIOA
#define VN_IN_PIN       GPIO_PIN_6

#define VN_IN_ADC_CH    ADC_CHANNEL_6
/* Freq exti */
#define FREQ_IN_PORT        GPIOB
#define FREQ_IN_PORT_RCU    RCU_GPIOB
#define FREQ_IN_PIN         GPIO_PIN_6

#define CH_NUM              (4)
#define SAMPLE_NUM          (100)

/* ADC转换完成后，该指针指向存放ADC原始数据的DMA缓冲区 */
__IO uint16_t (*g_p_adc2_buff)[2] = NULL;

uint16_t g_Vrefint = 0;  // 芯片内部1.2V参考电压的 ADC 原始值

// adc 采样数据DMA缓冲区
__attribute((used)) uint16_t adc2_buff[100][2];

void freq_exti_config(void)
{
    /* enable the GPIO clock */
    rcu_periph_clock_enable(FREQ_IN_PORT_RCU);
    rcu_periph_clock_enable(RCU_AF);
    /* configure GPIO pin as input */
    gpio_init(FREQ_IN_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_MAX, FREQ_IN_PIN);
    /* enable and set key EXTI interrupt to the lowest priority */
    nvic_irq_enable(EXTI5_9_IRQn, 5U, 0U);
    /* connect key EXTI line to key GPIO pin */
    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOB, GPIO_PIN_SOURCE_6);
    /* configure key EXTI line */
    exti_init(EXTI_6, EXTI_INTERRUPT, EXTI_TRIG_RISING);
    exti_interrupt_flag_clear(EXTI_6);
}

void timer7_trgo_config(uint16_t f){
    timer_parameter_struct timer_initpara;      // 定时器基本参数

    rcu_periph_clock_enable(RCU_TIMER7);
    timer_deinit(TIMER7);

    /* TIMER configuration */
    timer_initpara.prescaler         = ((timer_source_clock_get(TIMER7)/1000000U)-1); // TIMERxCLK(TIMERx_CK/PSC) is 100KHz
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_DOWN;
    timer_initpara.period            = (1000000U/f)-1;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(TIMER7, &timer_initpara);
    
    timer_update_event_enable(TIMER7);                                  // 配置TIMERx_CTL0的UPDIS
    timer_single_pulse_mode_config(TIMER7, TIMER_SP_MODE_REPETITIVE);   // 配置TIMERx_CTL0的SPM(配置为连续模式)
    timer_update_source_config(TIMER7, TIMER_UPDATE_SRC_GLOBAL);        // 配置TIMERx_CTL0的UPS
    
    timer_primary_output_config(TIMER7, ENABLE);
    /* auto-reload preload enable */
    timer_auto_reload_shadow_enable(TIMER7);
    /* auto-reload preload enable */
    // timer_enable(TIMER7);
}

/**
 * @brief   timer1 pwm初始化
 * @param   f 定时器更新频率, 单位Hz(2 - 1000000)
 * @note    TIMER1CLK(TIMER1_CK/PSC)固定为1000 000Hz(1MHz), 通过这个算出PSC寄存器的数值
*/
void timer1_pwm_config(uint16_t f)
{
    timer_parameter_struct timer_initpara;      // 定时器基本参数
    timer_oc_parameter_struct timer_ocintpara;  // 定时器输出设置

    rcu_periph_clock_enable(RCU_TIMER1);

    timer_deinit(TIMER1);
    /* TIMER configuration */
    timer_initpara.prescaler         = ((timer_source_clock_get(TIMER1)/1000000U)-1); // TIMER2CLK(TIMER2_CK/PSC) is 100KHz
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_DOWN;
    timer_initpara.period            = (1000000U/f)-1;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(TIMER1,&timer_initpara);

    /* CH0 configuration in PWM mode0 */
    timer_ocintpara.ocpolarity  = TIMER_OC_POLARITY_HIGH;
    timer_ocintpara.outputstate = TIMER_CCX_ENABLE;
    timer_channel_output_config(TIMER1, TIMER_CH_2, &timer_ocintpara);

    timer_channel_output_pulse_value_config(TIMER1, TIMER_CH_2, 100);
    timer_channel_output_mode_config(TIMER1, TIMER_CH_2, TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(TIMER1, TIMER_CH_2, TIMER_OC_SHADOW_DISABLE);

    timer_update_event_enable(TIMER1);                                  // 配置TIMERx_CTL0的UPDIS
    timer_single_pulse_mode_config(TIMER1, TIMER_SP_MODE_REPETITIVE);   // 配置TIMERx_CTL0的SPM(配置为连续模式)
    timer_update_source_config(TIMER1, TIMER_UPDATE_SRC_REGULAR);       // 配置TIMERx_CTL0的UPS

    /* TIMER1 primary output enable */
    timer_primary_output_config(TIMER1, ENABLE);
    /* auto-reload preload enable */
    timer_auto_reload_shadow_enable(TIMER1); 

    /* auto-reload preload enable */
    timer_enable(TIMER1);
}

/**
 * @brief   ADC DMA配置
*/
static void adc2_dma_config(void)
{
    /* ADC_DMA_channel configuration */
    dma_parameter_struct dma_data_parameter;
    
    /* ADC_DMA_channel deinit */
    dma_deinit(DMA1, DMA_CH4);

    rcu_periph_clock_enable(RCU_DMA1);
    /* initialize DMA single data mode */
    dma_data_parameter.periph_addr  = (uint32_t)(&ADC_RDATA(ADC2));
    dma_data_parameter.periph_inc   = DMA_PERIPH_INCREASE_DISABLE;
    dma_data_parameter.memory_addr  = (uint32_t)(adc2_buff);
    dma_data_parameter.memory_inc   = DMA_MEMORY_INCREASE_ENABLE;
    dma_data_parameter.periph_width = DMA_PERIPHERAL_WIDTH_16BIT;
    dma_data_parameter.memory_width = DMA_MEMORY_WIDTH_16BIT;
    dma_data_parameter.direction    = DMA_PERIPHERAL_TO_MEMORY;
    dma_data_parameter.number       = 100*2;
    dma_data_parameter.priority     = DMA_PRIORITY_HIGH;
    dma_flag_clear(DMA1, DMA_CH4, DMA_FLAG_HTF|DMA_FLAG_FTF);
    dma_init(DMA1, DMA_CH4, &dma_data_parameter);
  
    dma_circulation_enable(DMA1, DMA_CH4);
    dma_interrupt_enable(DMA1,DMA_CH4, DMA_CHXCTL_FTFIE);
    nvic_irq_enable(DMA1_Channel3_Channel4_IRQn,5,0);
    /* enable DMA channel */
    dma_channel_enable(DMA1, DMA_CH4);
}

void DMA1_Channel3_4_IRQHandler(void)
{
    if(dma_interrupt_flag_get(DMA1, DMA_CH4, DMA_INT_FLAG_FTF)){
        dma_interrupt_flag_clear(DMA1, DMA_CH4, DMA_INT_FLAG_FTF);
        g_p_adc2_buff = adc2_buff;
        exti_interrupt_enable(EXTI_6);
        timer_disable(TIMER1);
        timer_counter_value_config(TIMER1, 0);
    }
}

/**
 * @brief   电压采集通道
 */
void evse_adc_voltage_config(void)
{
    adc_deinit(ADC2);

    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_init(GPIOA, GPIO_MODE_AIN, GPIO_OSPEED_MAX, GPIO_PIN_1);
    gpio_init(GPIOA, GPIO_MODE_AIN, GPIO_OSPEED_MAX, GPIO_PIN_3);
    
    rcu_periph_clock_enable(RCU_AF);
    rcu_periph_clock_enable(RCU_ADC2);
    // gpio_pin_remap_config(GPIO_ADC1_ETRGREG_REMAP, ENABLE);
    /* config ADC clock */
    rcu_adc_clock_config(RCU_CKADC_CKAPB2_DIV2);
    /* ADC mode config */
    adc_mode_config(ADC_MODE_FREE);
    /* ADC data alignment config */
    adc_data_alignment_config(ADC2, ADC_DATAALIGN_RIGHT);
    /* ADC SCAN function enable */
    adc_special_function_config(ADC2, ADC_SCAN_MODE, ENABLE);
    /* 关闭连续模式(触发一次转换一次) */
    adc_special_function_config(ADC2, ADC_CONTINUOUS_MODE, DISABLE);

    adc_channel_length_config(ADC2, ADC_REGULAR_CHANNEL, 2);
    adc_regular_channel_config(ADC2, 0, ADC_CHANNEL_1, ADC_SAMPLETIME_239POINT5);
    adc_regular_channel_config(ADC2, 1, ADC_CHANNEL_3, ADC_SAMPLETIME_239POINT5);

    /* ADC trigger config */
    adc_external_trigger_source_config(ADC2, ADC_REGULAR_CHANNEL, ADC2_EXTTRIG_REGULAR_T1_CH2);
    /* ADC external trigger enable */
    adc_external_trigger_config(ADC2, ADC_REGULAR_CHANNEL, ENABLE);
    
    /* enable ADC interface */
    adc_enable(ADC2);
    bos_delay_ms(1);
    /* ADC calibration and reset calibration */
    adc_calibration_enable(ADC2);
    bos_delay_ms(1);
    adc_dma_mode_enable(ADC2);

    adc2_dma_config();
}

__IO uint8_t exti6_flag = false; // 正弦波一个周期开始的标志
/*!
    \brief      this function handles external lines 10 to 15 interrupt request
    \param[in]  none
    \param[out] none
    \retval     none
*/
// void EXTI5_9_IRQHandler(void)
// {
//     if(RESET != exti_interrupt_flag_get(EXTI_6)) {
//         // exti6_flag = true;
//         exti_interrupt_disable(EXTI_6);
//         dma_transfer_number_config(DMA1, DMA_CH4, 200);  // DMA重新开始计数
//         timer_enable(TIMER1);
//     }
//     exti_interrupt_flag_clear(EXTI_6);
// }

/**
 * @brief   获取正弦波正半波电压的平均值
 */
uint16_t get_sin_vol(__IO uint16_t pBuff[][2], uint16_t length, uint16_t index)
{
    static uint16_t voltage;
    uint16_t base_voltage;
    uint16_t temp = 0;
    uint32_t sum = 0, count = 0;

    for(uint32_t i = 0; i < length; i++){
        sum += pBuff[i][index];
    }
    base_voltage = (uint16_t)(sum/length);

    for(uint32_t i = 0; i < length; i++){
        voltage += abs((int32_t)pBuff[i][index]-(int32_t)base_voltage);
    }
    voltage /= length;

    return voltage;
}

static void task_entry_voltage_sample(void *parameter)
{
    uint16_t l_voltage = 0, c_voltage = 0;
    evse_adc_voltage_config();
    freq_exti_config();
    timer1_pwm_config(5000);

    /* 等待vrefint读取完毕 */
    while (g_Vrefint == 0)
    {
        bos_delay_ms(1);
    }

    for(;;){
        if(g_p_adc2_buff != NULL) {
            // l_voltage = 0;
            // for(int i = 0; i < 50; i++) {
            //     l_voltage += g_p_adc2_buff[i][1];
            // }
            // l_voltage /= 50;
            l_voltage = get_sin_vol(g_p_adc2_buff, 100, 0);
            c_voltage = get_sin_vol(g_p_adc2_buff, 100, 1);
            // log_i("l_raw: %d, l_vol: %.3f", l_voltage, ((1.2*((float)l_voltage/(float)v_refint))*10000)/21.0 - 3.3);
            // log_i("c_raw: %d, c_vol: %.3f", c_voltage, 1.2*((float)c_voltage/(float)v_refint));
            log_i("c_raw: %d, c_vol: %.3f", c_voltage, ((1200.0*((float)c_voltage/(float)g_Vrefint))*2.0)/51.0);
            g_p_adc2_buff = NULL;
        }
        bos_delay_ms(1);
    }
}
// bos_task_export(voltage_sample, task_entry_voltage_sample, BOS_MAX_PRIORITY, NULL);

/**
 * @brief   温度采集通道
 */
void evse_adc_ntc_config(void)
{
    adc_deinit(ADC1);

    rcu_periph_clock_enable(ON_BOARD_NTC_PORT_RCU);
    rcu_periph_clock_enable(PLUG_NTC_PORT_RCU);
    gpio_init(ON_BOARD_NTC_PORT, GPIO_MODE_AIN, GPIO_OSPEED_MAX, ON_BOARD_NTC_PIN);
    gpio_init(PLUG_NTC_PORT, GPIO_MODE_AIN, GPIO_OSPEED_MAX, PLUG_NTC_PIN);

    rcu_periph_clock_enable(RCU_ADC1);
    /* ADC mode config */
    adc_mode_config(ADC_MODE_FREE);
    /* ADC data alignment config */
    adc_data_alignment_config(ADC1, ADC_DATAALIGN_RIGHT);
    /* ADC SCAN function enable */
    adc_special_function_config(ADC1, ADC_SCAN_MODE, ENABLE);

    adc_channel_length_config(ADC1, ADC_INSERTED_CHANNEL, 2);
    adc_inserted_channel_config(ADC1, 0, ON_BOARD_NTC_ADC_CH, ADC_SAMPLETIME_239POINT5);
    adc_inserted_channel_config(ADC1, 1, PLUG_NTC_ADC_CH, ADC_SAMPLETIME_239POINT5);

    /* ADC external trigger enable */
    adc_external_trigger_config(ADC1, ADC_INSERTED_CHANNEL, ENABLE);
    /* ADC trigger config */
    adc_external_trigger_source_config(ADC1, ADC_INSERTED_CHANNEL, ADC0_1_2_EXTTRIG_INSERTED_NONE);
    
    /* enable ADC interface */
    adc_enable(ADC1);
    bos_delay_ms(1);
    /* ADC calibration and reset calibration */
    adc_calibration_enable(ADC1);
}

static void task_entry_ntc_sample(void *parameter)
{
    evse_adc_ntc_config();
    for(;;){
        bos_delay_ms(500);
        adc_software_trigger_enable(ADC1, ADC_INSERTED_CHANNEL);
        while(adc_flag_get(ADC1, ADC_FLAG_EOIC) == RESET){}
        adc_flag_clear(ADC1, ADC_FLAG_EOIC);
        log_d("on_board ntc: %d", ADC_IDATA0(ADC1));
        log_d("plug ntc: %d", ADC_IDATA1(ADC1));
    }
}
// bos_task_export(ntc_sample, task_entry_ntc_sample, BOS_MAX_PRIORITY, NULL);
