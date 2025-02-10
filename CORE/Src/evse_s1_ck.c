#include "evse_s1_ck.h"
#include "basic_os.h"

#define LOG_TAG "evse.s1_ck"
#include "elog.h"

// 车端二极管检测
#define S1_CK_PORT     GPIOA
#define S1_CK_RCU      RCU_GPIOA
#define S1_CK_PIN      GPIO_PIN_2

void evse_s1_ck_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_init(GPIOA, GPIO_MODE_AIN, GPIO_OSPEED_MAX, GPIO_PIN_2);

    adc_channel_length_config(ADC1, ADC_REGULAR_CHANNEL, 1);
    adc_regular_channel_config(ADC1, 0, ADC_CHANNEL_2, ADC_SAMPLETIME_239POINT5);

    /* ADC external trigger enable */
    adc_external_trigger_config(ADC1, ADC_REGULAR_CHANNEL, ENABLE);
    /* ADC trigger config */
    adc_external_trigger_source_config(ADC1, ADC_REGULAR_CHANNEL, ADC0_1_2_EXTTRIG_INSERTED_NONE);
}

ErrStatus evse_s1_ck(void)
{
    uint32_t adc_data = 0;
    adc_software_trigger_enable(ADC1, ADC_REGULAR_CHANNEL);
    for(uint8_t i = 0; i < 10; i++){
        while(adc_flag_get(ADC1, ADC_FLAG_EOC) == RESET){}
        adc_data += adc_regular_data_read(ADC1);
    }
    adc_data /= 10;
    log_d("adc_data: %d", adc_data);
    return SUCCESS;
}
