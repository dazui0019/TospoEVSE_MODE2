#include "evse_ntc.h"
#include "basic_os.h"
#include "EventRecorder.h"
#include "drv_timer.h"
#include "evse_charge.h"

#define LOG_TAG "evse.ntc"
#include "elog.h"

/* 板载NTC(NTC0) */
#define ON_BOARD_NTC_PORT       GPIOC
#define ON_BOARD_NTC_PORT_RCU   RCU_GPIOC
#define ON_BOARD_NTC_PIN        GPIO_PIN_1
#define ON_BOARD_NTC_ADC_CH     ADC_CHANNEL_11
/* 电源插头NTC(NTC1) */
#define PLUG_NTC_PORT           GPIOC
#define PLUG_NTC_PORT_RCU       RCU_GPIOC
#define PLUG_NTC_PIN            GPIO_PIN_2
#define PLUG_NTC_ADC_CH         ADC_CHANNEL_12

/**
 * @brief   温度采集通道
 */
void evse_ntc_init(void)
{
    adc_deinit(ADC1);

    rcu_periph_clock_enable(ON_BOARD_NTC_PORT_RCU);
    // rcu_periph_clock_enable(PLUG_NTC_PORT_RCU);
    gpio_init(ON_BOARD_NTC_PORT, GPIO_MODE_AIN, GPIO_OSPEED_MAX, ON_BOARD_NTC_PIN);
    // gpio_init(PLUG_NTC_PORT, GPIO_MODE_AIN, GPIO_OSPEED_MAX, PLUG_NTC_PIN);

    rcu_periph_clock_enable(RCU_ADC1);
    /* ADC mode config */
    adc_mode_config(ADC_MODE_FREE);
    /* ADC data alignment config */
    adc_data_alignment_config(ADC1, ADC_DATAALIGN_RIGHT);
    /* ADC SCAN function enable */
    adc_special_function_config(ADC1, ADC_SCAN_MODE, ENABLE);

    // adc_channel_length_config(ADC1, ADC_INSERTED_CHANNEL, 2);
    adc_channel_length_config(ADC1, ADC_INSERTED_CHANNEL, 1);
    adc_inserted_channel_config(ADC1, 0, ON_BOARD_NTC_ADC_CH, ADC_SAMPLETIME_239POINT5);
    // adc_inserted_channel_config(ADC1, 1, PLUG_NTC_ADC_CH, ADC_SAMPLETIME_239POINT5);

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

/**
 * @brief   用定时器(TIEMR3)触发ADC对NTC采样
 * @note    目前没用到
 */
void evse_ntc_timer_config(uint16_t f)
{
    timer_parameter_struct timer_initpara;      // 定时器基本参数

    rcu_periph_clock_enable(RCU_TIMER3);
    timer_deinit(TIMER3);

    /* TIMER configuration */
    timer_initpara.prescaler         = ((timer_source_clock_get(TIMER3)/100000U)-1); // TIMERxCLK(TIMERx_CK/PSC) is 1000KHz
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_DOWN;
    timer_initpara.period            = (100000U/f)-1;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(TIMER3, &timer_initpara);
    
    timer_update_event_enable(TIMER3);                                  // 配置TIMERx_CTL0的UPDIS
    timer_single_pulse_mode_config(TIMER3, TIMER_SP_MODE_REPETITIVE);   // 配置TIMERx_CTL0的SPM(配置为连续模式)
    timer_update_source_config(TIMER3, TIMER_UPDATE_SRC_GLOBAL);        // 配置TIMERx_CTL0的UPS
    
    timer_primary_output_config(TIMER3, ENABLE);
    /* auto-reload preload enable */
    timer_auto_reload_shadow_enable(TIMER3);
    // timer_enable(TIMER3);
}

void evse_ntc_get_raw(uint16_t *ob_raw, uint16_t *pl_raw)
{
    static uint16_t ob_ntc = 0, pl_ntc = 0;
    // adc_flag_clear(ADC1, ADC_FLAG_EOIC);
    ADC_STAT(ADC1) = ~((uint32_t)ADC_FLAG_EOIC);
    
    // adc_software_trigger_enable(ADC1, ADC_INSERTED_CHANNEL);
    ADC_CTL1(ADC1) |= ADC_CTL1_SWICST;
    
    // while (adc_flag_get(ADC1, ADC_FLAG_EOIC) == RESET){}
    while((ADC_STAT(ADC1) & ADC_FLAG_EOIC) == RESET){}
    // adc_flag_clear(ADC1, ADC_FLAG_EOIC);
    ADC_STAT(ADC1) = ~((uint32_t)ADC_FLAG_EOIC);

    ob_ntc = 0.4f*(float)ob_ntc + 0.6f*(float)ADC_IDATA0(ADC1);
    pl_ntc = 0.4f*(float)pl_ntc + 0.6f*(float)ADC_IDATA1(ADC1);

    *ob_raw = ob_ntc;
    *pl_raw = pl_ntc;
}

void evse_ntc_get_raw_ob(uint16_t* ob_raw)
{
    static uint16_t ob_ntc = 0;
    // adc_flag_clear(ADC1, ADC_FLAG_EOIC);
    ADC_STAT(ADC1) = ~((uint32_t)ADC_FLAG_EOIC);
    
    // adc_software_trigger_enable(ADC1, ADC_INSERTED_CHANNEL);
    ADC_CTL1(ADC1) |= ADC_CTL1_SWICST;
    
    // while (adc_flag_get(ADC1, ADC_FLAG_EOIC) == RESET){}
    while((ADC_STAT(ADC1) & ADC_FLAG_EOIC) == RESET){}
    // adc_flag_clear(ADC1, ADC_FLAG_EOIC);
    ADC_STAT(ADC1) = ~((uint32_t)ADC_FLAG_EOIC);

    ob_ntc = 0.4f*(float)ob_ntc + 0.6f*(float)ADC_IDATA0(ADC1);

    *ob_raw = ob_ntc;
}

extern __IO uint16_t g_Vrefint;  // evse_ac

static void task_entry_ntc_sample(void *parameter)
{
    uint16_t ob_ntc, overheat_cnt = 0;
    static uint8_t overheat_flag = false;

    /* 等待充电桩主任务完成初始化 */
    while (evse_get_state() == EVSE_REBOOT)
    {
        bos_delay_ms(100);
    }

    for(;;){
        EventStartA(2);
        // evse_ntc_get_raw(&ob_ntc, &pl_ntc);
        evse_ntc_get_raw_ob(&ob_ntc);
        EventStopA(2);

        // log_d("ob_ntc: %d, pl_ntc: %d", ob_ntc, pl_ntc);
        log_d("ob_ntc: %d", ob_ntc);

        if(ob_ntc < 817 && overheat_flag == false){   // 70°C
            if(overheat_cnt++ > 10){
                overheat_flag = true;
                evse_set_fault_flag(FAULT_OVER_HEAT);
                log_e("overheat: %d", ob_ntc);
            }
        }else if (ob_ntc > 1112 && overheat_flag == true){    // 60°C
            overheat_cnt = 0;
            overheat_flag = false;
            evse_clear_fault_flag(FAULT_OVER_HEAT);
            log_i("clear overheat flag: %d", ob_ntc);
        }
        bos_delay_ms(1000);
    }
}
bos_task_export(ntc_sample, task_entry_ntc_sample, BOS_MAX_PRIORITY, NULL);
