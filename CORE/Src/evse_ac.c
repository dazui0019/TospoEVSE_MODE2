/**
 * @file        evse_ac.c
 * @brief       充电桩的ADC采样部分, 除了CP和接地检测以外, 全在这个文件里
 * @todo        后面CP和接地检测部分也可以放在这个文件里
 */
#include "evse_ac.h"
#include "basic_os.h"
#include "drv_timer.h"
#include "printf.h"
#include "evse_cp.h"

#define LOG_TAG "evse.adc"
#include "elog.h"

#define AC_ADC          ADC0
#define AC_ADC_RCU      RCU_ADC0
/* 电流 */
#define CURRENT_PORT            GPIOA
#define CURRENT_PORT_RCU        RCU_GPIOA
#define CURRENT_PIN             GPIO_PIN_3
#define CURRENT_ADC_CH          ADC_CHANNEL_3
/* 火线进线电压 */
#define VL_PORT                 GPIOA
#define VL_PORT_RCU             RCU_GPIOA
#define VL_PIN                  GPIO_PIN_4
#define VL_ADC_CH               ADC_CHANNEL_4
/* 接地检测 */      
#define PE_PORT                 GPIOA
#define PE_PORT_RCU             RCU_GPIOA
#define PE_PIN                  GPIO_PIN_7
#define PE_ADC_CH               ADC_CHANNEL_7
/* Freq exti */
#define TRIG_PORT               GPIOB
#define TRIG_PORT_RCU           RCU_GPIOB
#define TRIG_PIN                GPIO_PIN_6

#define CH_NUM                  (3)
#define SAMPLE_NUM              (100)

__IO uint8_t g_vol_error_flag = false;
__IO uint8_t g_pe_error_flag = false;

__IO uint16_t g_Vrefint = 0;  // 芯片内部1.2V参考电压的 ADC 原始值

// adc 采样数据DMA缓冲区
__attribute((used)) uint16_t ac_adc_buff[100][3];

/**
 * @brief   获取芯片内部1.2V基准电压值
 */
void adc_verf_config(void)
{
    adc_deinit(ADC0);
    rcu_periph_clock_enable(RCU_ADC0);
    /* ADC mode config */
    adc_mode_config(ADC_MODE_FREE);
    /* ADC data alignment config */
    adc_data_alignment_config(ADC0, ADC_DATAALIGN_RIGHT);
    /* ADC SCAN function enable */
    adc_special_function_config(ADC0, ADC_SCAN_MODE, DISABLE);

    /* ADC channel length config */
    adc_channel_length_config(ADC0, ADC_INSERTED_CHANNEL, 1);
    /* ADC internal reference voltage channel config */
    adc_inserted_channel_config(ADC0, 0, ADC_CHANNEL_17, ADC_SAMPLETIME_239POINT5);

    /* ADC external trigger enable */
    adc_external_trigger_config(ADC0, ADC_INSERTED_CHANNEL, ENABLE);
    /* ADC trigger config */
    adc_external_trigger_source_config(ADC0, ADC_INSERTED_CHANNEL, ADC0_1_2_EXTTRIG_INSERTED_NONE);

    /* ADC temperature and Vrefint enable */
    adc_tempsensor_vrefint_enable();
    
    /* enable ADC interface */
    adc_enable(ADC0);
    bos_delay_ms(1);
    /* ADC calibration and reset calibration */
    adc_calibration_enable(ADC0);
}

void freq_exti_config(void)
{
    /* enable the GPIO clock */
    rcu_periph_clock_enable(TRIG_PORT_RCU);
    rcu_periph_clock_enable(RCU_AF);
    /* configure GPIO pin as input */
    gpio_init(TRIG_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_MAX, TRIG_PIN);
    /* enable and set key EXTI interrupt to the lowest priority */
    nvic_irq_enable(EXTI5_9_IRQn, 5U, 0U);
    /* connect key EXTI line to key GPIO pin */
    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOB, GPIO_PIN_SOURCE_6);
    
    exti_interrupt_flag_clear(EXTI_6);
    /* configure key EXTI line */
    exti_init(EXTI_6, EXTI_INTERRUPT, EXTI_TRIG_RISING);
}

/**
 * @brief   timer1 pwm(TIMER1_CH1)初始化(用来触发adc采样)
 * @param   f 定时器更新频率, 单位Hz(2 - 1000000)
 * @note    TIMER1CLK(TIMER1_CK/PSC)固定为1000 000Hz(1MHz), 通过这个算出PSC寄存器的数值
*/
void evse_ac_timer_config(uint16_t f)
{
    timer_parameter_struct timer_initpara;      // 定时器基本参数
    timer_oc_parameter_struct timer_ocintpara;  // 定时器输出设置

    rcu_periph_clock_enable(RCU_TIMER1);

    timer_deinit(TIMER1);
    /* TIMER configuration */
    timer_initpara.prescaler         = ((timer_source_clock_get(TIMER1)/1000000U)-1); // TIMER2CLK(TIMER2_CK/PSC) is 100KHz
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = (1000000U/f)-1;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(TIMER1,&timer_initpara);

    /* CH0 configuration in PWM mode0 */
    timer_ocintpara.ocpolarity  = TIMER_OC_POLARITY_HIGH;
    timer_ocintpara.outputstate = TIMER_CCX_ENABLE;
    timer_channel_output_config(TIMER1, TIMER_CH_1, &timer_ocintpara);

    timer_channel_output_pulse_value_config(TIMER1, TIMER_CH_1, 1);
    timer_channel_output_mode_config(TIMER1, TIMER_CH_1, TIMER_OC_MODE_PWM1);
    timer_channel_output_shadow_config(TIMER1, TIMER_CH_1, TIMER_OC_SHADOW_ENABLE);

    timer_update_event_enable(TIMER1);                                  // 配置TIMERx_CTL0的UPDIS
    timer_single_pulse_mode_config(TIMER1, TIMER_SP_MODE_REPETITIVE);   // 配置TIMERx_CTL0的SPM(配置为连续模式)
    timer_update_source_config(TIMER1, TIMER_UPDATE_SRC_REGULAR);       // 配置TIMERx_CTL0的UPS

    /* TIMER1 primary output enable */
    timer_primary_output_config(TIMER1, ENABLE);
    /* auto-reload preload enable */
    timer_auto_reload_shadow_enable(TIMER1); 

    /* auto-reload preload enable */
    timer_disable(TIMER1);
}

/**
 * @brief   ADC DMA配置
*/
static void adc0_dma_config(uint16_t number)
{
    /* ADC_DMA_channel configuration */
    dma_parameter_struct dma_data_parameter;
    
    /* ADC_DMA_channel deinit */
    dma_deinit(DMA0, DMA_CH0);

    rcu_periph_clock_enable(RCU_DMA0);
    /* initialize DMA single data mode */
    dma_data_parameter.periph_addr  = (uint32_t)(&ADC_RDATA(ADC0));
    dma_data_parameter.periph_inc   = DMA_PERIPH_INCREASE_DISABLE;
    dma_data_parameter.memory_addr  = (uint32_t)(ac_adc_buff);
    dma_data_parameter.memory_inc   = DMA_MEMORY_INCREASE_ENABLE;
    dma_data_parameter.periph_width = DMA_PERIPHERAL_WIDTH_16BIT;
    dma_data_parameter.memory_width = DMA_MEMORY_WIDTH_16BIT;
    dma_data_parameter.direction    = DMA_PERIPHERAL_TO_MEMORY;
    dma_data_parameter.number       = number;
    dma_data_parameter.priority     = DMA_PRIORITY_HIGH;
    dma_flag_clear(DMA0, DMA_CH0, DMA_FLAG_HTF|DMA_FLAG_FTF);
    dma_init(DMA0, DMA_CH0, &dma_data_parameter);
  
    dma_circulation_enable(DMA0, DMA_CH0);
    dma_interrupt_flag_clear(DMA0, DMA_CH0, DMA_INT_FLAG_HTF|DMA_INT_FLAG_FTF);
    dma_interrupt_enable(DMA0,DMA_CH0, DMA_CHXCTL_FTFIE);
    nvic_irq_enable(DMA0_Channel0_IRQn,5,0);
    /* enable DMA channel */
    dma_channel_enable(DMA0, DMA_CH0);
}

/**
 * @brief   电压采集通道
 */
void evse_ac_adc_config(void)
{
    adc_deinit(AC_ADC);

    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_init(GPIOA, GPIO_MODE_AIN, GPIO_OSPEED_MAX, GPIO_PIN_3);   // L1电流
    gpio_init(GPIOA, GPIO_MODE_AIN, GPIO_OSPEED_MAX, GPIO_PIN_4);   // L1电压
    gpio_init(GPIOA, GPIO_MODE_AIN, GPIO_OSPEED_MAX, GPIO_PIN_7);   // PE

    rcu_periph_clock_enable(RCU_AF);
    rcu_periph_clock_enable(AC_ADC_RCU);
    /* config ADC clock */
    rcu_adc_clock_config(RCU_CKADC_CKAPB2_DIV2);
    /* ADC mode config */
    adc_mode_config(ADC_MODE_FREE);
    /* ADC data alignment config */
    adc_data_alignment_config(AC_ADC, ADC_DATAALIGN_RIGHT);
    /* ADC SCAN function enable */
    adc_special_function_config(AC_ADC, ADC_SCAN_MODE, ENABLE);
    /* 关闭连续模式(触发一次转换一次) */
    adc_special_function_config(AC_ADC, ADC_CONTINUOUS_MODE, DISABLE);

    adc_channel_length_config(AC_ADC, ADC_REGULAR_CHANNEL, 3);
    adc_regular_channel_config(AC_ADC, 0, ADC_CHANNEL_3, ADC_SAMPLETIME_71POINT5);
    adc_regular_channel_config(AC_ADC, 1, ADC_CHANNEL_4, ADC_SAMPLETIME_71POINT5);
    adc_regular_channel_config(AC_ADC, 2, ADC_CHANNEL_7, ADC_SAMPLETIME_71POINT5);

    /* ADC trigger config */
    adc_external_trigger_source_config(AC_ADC, ADC_REGULAR_CHANNEL, ADC0_1_EXTTRIG_REGULAR_T1_CH1);
    /* ADC external trigger enable */
    adc_external_trigger_config(AC_ADC, ADC_REGULAR_CHANNEL, ENABLE);

    /* enable ADC interface */
    adc_enable(AC_ADC);
    bos_delay_ms(1);
    /* ADC calibration and reset calibration */
    adc_calibration_enable(AC_ADC);
    bos_delay_ms(1);
    adc_dma_mode_enable(AC_ADC);

    adc0_dma_config(CH_NUM*SAMPLE_NUM);
}

__IO uint8_t start_flag = false; // 正弦波一个周期开始的标志
__IO uint8_t cplt_flag = false;
/*!
    \brief      this function handles external lines 10 to 15 interrupt request
    \param[in]  none
    \param[out] none
    \retval     none
*/
void EXTI5_9_IRQHandler(void)
{
    if(RESET != (EXTI_PD & (uint32_t)EXTI_6)){   // 开启电压采集: 开启定时器
        TIMER_CNT(TIMER1) = (uint32_t)0xC7; // timer_counter_value_config(TIMER1, 0xC7);
        TIMER_CTL0(TIMER1) |= (uint32_t)TIMER_CTL0_CEN; // timer_enable(TIMER1);
        EXTI_PD = (uint32_t)EXTI_6;         // exti_interrupt_flag_clear(EXTI_6);
    }
}

void DMA0_Channel0_IRQHandler(void)
{
    if(dma_interrupt_flag_get(DMA0, DMA_CH0, DMA_INT_FLAG_FTF)){    // 完成采样后, 先暂停采样
        DMA_INTC(DMA0) |= DMA_FLAG_ADD(DMA_INT_FLAG_FTF, DMA_CH0); // dma_interrupt_flag_clear(DMA0, DMA_CH0, DMA_INT_FLAG_FTF);
        TIMER_CTL0(TIMER1) &= ~(uint32_t)TIMER_CTL0_CEN;    // timer_disable(TIMER1);
        EXTI_INTEN &= ~(uint32_t)EXTI_6;    // exti_interrupt_disable(EXTI_6);

        cplt_flag = true;
    }
}

/**
 * @brief   获取正弦波正半波的平均值
 */
uint16_t get_sin_val(__IO uint16_t pBuff[][CH_NUM], uint16_t length, uint16_t index)
{
    static uint32_t val;
    uint16_t base_val;
    uint16_t temp = 0;
    uint32_t sum = 0, count = 0;

    if(index > CH_NUM){
        log_e("index > CH_NUM");
        return 65535;
    }

    for(uint32_t i = 0; i < length; i++){
        sum += pBuff[i][index];
    }
    base_val = (uint16_t)(sum/length);

    for(uint32_t i = 0; i < length; i++){
        val += abs((int32_t)pBuff[i][index]-(int32_t)base_val);
        // log_d("val: %d", abs((int32_t)pBuff[i][index]-(int32_t)base_val));
    }

    // log_d("base_val: %d, val: %d", base_val, val);
    val /= length;

    return (uint16_t)val;
}

static void task_entry_voltage_sample(void *parameter)
{
    uint16_t pe_val, l1_val = 0, c_val = 0;
    
    /* 等待vrefint读取完毕 */
    while (g_Vrefint == 0)
    {
        bos_delay_ms(1);
    }

    evse_ac_adc_config();   // ac_adc和g_Vrefint共用ADC0, 所以等g_Vrefint获取完成后, 再重新配置ADC0给ac_adc用
    freq_exti_config();
    evse_ac_timer_config(5000); // 5000Hz/50Hz = 100个

    exti_interrupt_enable(EXTI_6);

    for(;;){
        if(cplt_flag == true){
            cplt_flag = false;
            pe_val = get_sin_val(ac_adc_buff, SAMPLE_NUM, 2);
            l1_val = get_sin_val(ac_adc_buff, SAMPLE_NUM, 1);
            c_val = get_sin_val(ac_adc_buff, SAMPLE_NUM, 0);
            
            /* 设置过(欠)压标志位(±15%: 187 - 253) */
            if(g_vol_error_flag == false && (l1_val >= 674 || l1_val <= 492)){
                g_vol_error_flag = true;
                log_e("voltage error: %d", l1_val);
            }else if(g_vol_error_flag == true && (l1_val >= 513 && l1_val <= 639)){
                g_vol_error_flag = false;
                log_i("clear voltage error flag: %d", l1_val);
            }

            if(g_pe_error_flag == false && pe_val >= 120){
                g_pe_error_flag = true;
                log_e("PE error: %d", pe_val);
            }else if(g_pe_error_flag == true && pe_val <= 10){
                g_pe_error_flag = false;
                log_i("Clear PE error flag: %d", pe_val);
            }
            // log_i("pe_val: %d, l1_val: %d, c_val: %d", pe_val, l1_val, c_val);

            // log_i("l_raw: %d, l_vol: %.3f", l1_val, ((1.2*((float)l1_val/(float)g_Vrefint))*10000)/21.0 - 3.3);
            // log_i("c_raw: %d, c_vol: %.3f", c_voltage, ((1200.0*((float)c_voltage/(float)g_Vrefint))*2.0)/51.0);
            exti_interrupt_flag_clear(EXTI_6);
            exti_interrupt_enable(EXTI_6);
        }
        bos_delay_ms(1);
    }
}
bos_task_export(voltage_sample, task_entry_voltage_sample, BOS_MAX_PRIORITY, NULL);
