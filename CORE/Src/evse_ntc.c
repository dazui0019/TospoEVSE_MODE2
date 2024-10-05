#include "evse_ntc.h"
#include "basic_os.h"
#include "EventRecorder.h"

#define LOG_TAG "evse.ntc"
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

/**
 * @brief   温度采集通道
 */
void evse_ntc_config(void)
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
    // bos_delay_ms(1);
    /* ADC calibration and reset calibration */
    adc_calibration_enable(ADC1);
}

void evse_ntc_get_raw1(uint16_t *ob_raw, uint16_t *pl_raw)
{
    // adc_flag_clear(ADC1, ADC_FLAG_EOIC);
    ADC_STAT(ADC1) = ~((uint32_t)ADC_FLAG_EOIC);
    
    // adc_software_trigger_enable(ADC1, ADC_INSERTED_CHANNEL);
    ADC_CTL1(ADC1) |= ADC_CTL1_SWICST;
    
    // while (adc_flag_get(ADC1, ADC_FLAG_EOIC) == RESET){}
    while((ADC_STAT(ADC1) & ADC_FLAG_EOIC) == RESET){}
    // adc_flag_clear(ADC1, ADC_FLAG_EOIC);
    ADC_STAT(ADC1) = ~((uint32_t)ADC_FLAG_EOIC);

    *ob_raw = ADC_IDATA0(ADC1);
    *pl_raw = ADC_IDATA1(ADC1);
    
}

void evse_ntc_get_raw2(uint16_t *ob_raw, uint16_t *pl_raw)
{
    adc_flag_clear(ADC1, ADC_FLAG_EOIC);
    // ADC_STAT(ADC1) = ~((uint32_t)ADC_FLAG_EOIC);
    
    adc_software_trigger_enable(ADC1, ADC_INSERTED_CHANNEL);
    // ADC_CTL1(ADC1) |= ADC_CTL1_SWICST;
    
    while (adc_flag_get(ADC1, ADC_FLAG_EOIC) == RESET){}
    // while((ADC_STAT(ADC1) & ADC_FLAG_EOIC) == RESET){}
    adc_flag_clear(ADC1, ADC_FLAG_EOIC);
    // ADC_STAT(ADC1) = ~((uint32_t)ADC_FLAG_EOIC);

    *ob_raw = ADC_IDATA0(ADC1);
    *pl_raw = ADC_IDATA1(ADC1);
}

static void task_entry_ntc_sample(void *parameter)
{
    uint16_t ob_ntc, pl_ntc;

    evse_ntc_config();

    for(;;){
        EventStartA(0);
        evse_ntc_get_raw1(&ob_ntc, &pl_ntc);
        EventStopA(0);

        EventStartA(1);
        evse_ntc_get_raw2(&ob_ntc, &pl_ntc);
        EventStopA(1);

        log_d("ob=%d, pl=%d", ob_ntc, pl_ntc);
        bos_delay_ms(1000);
    }
}
bos_task_export(ntc_sample, task_entry_ntc_sample, BOS_MAX_PRIORITY, NULL);
