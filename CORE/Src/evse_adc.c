#include "evse_adc.h"
#include "basic_os.h"
#include "drv_timer.h"

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
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
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
    timer_enable(TIMER7);
}

/**
 * @brief   电压和电流采集通道
 */
void evse_adc_voltage_config(void)
{
    adc_deinit(ADC1);

    rcu_periph_clock_enable(VL_IN_PORT_RCU);
    gpio_init(VL_IN_PORT, GPIO_MODE_AIN, GPIO_OSPEED_MAX, VL_IN_PIN);
    
    rcu_periph_clock_enable(RCU_AF);
    rcu_periph_clock_enable(RCU_ADC1);
    gpio_pin_remap_config(GPIO_ADC1_ETRGREG_REMAP, ENABLE);
    /* config ADC clock */
    rcu_adc_clock_config(RCU_CKADC_CKAPB2_DIV2);
    /* ADC mode config */
    adc_mode_config(ADC_MODE_FREE);
    /* ADC data alignment config */
    adc_data_alignment_config(ADC1, ADC_DATAALIGN_RIGHT);
    /* ADC SCAN function enable */
    adc_special_function_config(ADC1, ADC_SCAN_MODE, DISABLE);
    /* 关闭连续模式(触发一次转换一次) */
    adc_special_function_config(ADC1, ADC_CONTINUOUS_MODE, DISABLE);

    adc_channel_length_config(ADC1, ADC_REGULAR_CHANNEL, 1);
    adc_regular_channel_config(ADC1, 0, VL_IN_ADC_CH, ADC_SAMPLETIME_71POINT5);

    /* ADC trigger config */
    adc_external_trigger_source_config(ADC1, ADC_REGULAR_CHANNEL, ADC0_1_EXTTRIG_REGULAR_T7_TRGO);
    /* ADC external trigger enable */
    adc_external_trigger_config(ADC1, ADC_REGULAR_CHANNEL, ENABLE);
    
    /* enable ADC interface */
    adc_enable(ADC1);
    bos_delay_ms(1);
    /* ADC calibration and reset calibration */
    adc_calibration_enable(ADC1);
}

__IO uint8_t exti6_flag = false; // 正弦波一个周期开始的标志
/*!
    \brief      this function handles external lines 10 to 15 interrupt request
    \param[in]  none
    \param[out] none
    \retval     none
*/
void EXTI5_9_IRQHandler(void)
{
    if(RESET != exti_interrupt_flag_get(EXTI_6)) {
        exti6_flag = true;
    }
    exti_interrupt_flag_clear(EXTI_6);
}

static void task_entry_voltage_sample(void *parameter)
{
    evse_adc_voltage_config();
    freq_exti_config();
    timer7_trgo_config(500);

    timer_master_slave_mode_config(TIMER7, TIMER_MASTER_SLAVE_MODE_ENABLE);
    timer_master_output_trigger_source_select(TIMER7, TIMER_TRI_OUT_SRC_UPDATE);	// 更新事件作为TRGO的触发信号源

    for(;;){
        // if(exti6_flag) {
        //     exti6_flag = false;
        // }
        // adc_software_trigger_enable(ADC1, ADC_REGULAR_CHANNEL);
        bos_delay_ms(100);
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
