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
#include "evse_charge.h"
#include "evse_ui.h"

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
#define TRIG_PORT               GPIOA
#define TRIG_PORT_RCU           RCU_GPIOA
#define TRIG_PIN                GPIO_PIN_6
#define TRIG_SOURCE_PORT        GPIO_PORT_SOURCE_GPIOA
#define TRIG_SOURCE_PIN         GPIO_PIN_SOURCE_6

#define CH_NUM                  (3)
#define SAMPLE_NUM              (100)

/* 全局变量 */
__IO uint8_t  g_over_cur_flag = false;
__IO uint8_t  g_over_vol_flag = false;
__IO uint8_t  g_under_vol_flag = false;
__IO uint8_t g_pe_error_flag = false;
__IO uint16_t g_Vrefint = 0;  // 芯片内部1.2V参考电压的 ADC 原始值
__IO double g_kwh = 0;

/* 中断标志位 */
static __IO uint8_t start_flag = false; // 正弦波一个周期开始的标志
static __IO uint8_t cplt_flag = false;
extern __IO uint8_t second_flag;

// adc 采样数据DMA缓冲区
__attribute((used)) uint16_t ac_adc_buff[100][3];

/* 函数声明 */
static uint16_t get_sin_val(__IO const uint16_t pBuff[][CH_NUM], uint16_t length, uint16_t index);
static void evse_ac_adc_config(void);
static void evse_ac_timer_config(uint16_t f);
static void freq_exti_config(void);

float vol = 0.0f;
float cur = 0.0f;
float power = 0.0f;

static void task_entry_voltage_sample(void *parameter)
{
    uint16_t pe_val = 0, l1_val = 0, c_val = 0;
    uint8_t max_cur;

    uint16_t vol_err_cnt = 0;   // 电压错误计数
    uint16_t cur_err_cnt = 0;   // 电流错误计数
    uint16_t pe_err_cnt = 0;    // 接地错误计数

    /* 等待充电桩主任务完成初始化 */
    while (evse_get_state() == EVSE_REBOOT)
    {
        bos_delay_ms(10);
    }
    
    /* 初始化AC检测 */
    evse_ac_init();
    rtc_interrupt_enable(RTC_INT_SECOND);

    for(;;){
        if(cplt_flag == true){
            cplt_flag = false;
            pe_val = get_sin_val(ac_adc_buff, SAMPLE_NUM, 2);    // 一阶互补滤波
            l1_val = (0.6*l1_val)+(0.4*get_sin_val(ac_adc_buff, SAMPLE_NUM, 1));    // 一阶互补滤波
            c_val = (0.6*c_val)+(0.4*get_sin_val(ac_adc_buff, SAMPLE_NUM, 0));      // 一阶互补滤波
            
            cur = c_val/28.0f - 0.1428f;        // 转换成人类可读的数据
            vol = (l1_val*5.0f)/13.0f - 2.69f;  // 转换成人类可读的数据
            power = cur*vol;

            /* 设置过压标志位(Urms>253) */
            if(l1_val > 674 && g_over_vol_flag == false){
                if(vol_err_cnt++ > 2){
                    vol_err_cnt = 0;
                    g_over_vol_flag = true;
                    log_e("over_vol: %d", l1_val);
                }
            }else if(l1_val < 639 && g_over_vol_flag == true){ // 当Urms<242时清除过压标志
                if(vol_err_cnt++ > 2){
                    vol_err_cnt = 0;
                    g_over_vol_flag = false;
                    log_i("clear over_vol flag: %d", l1_val);
                }
            }
            /* 设置欠压标志(Urms<187) */
            if(l1_val < 492 && g_under_vol_flag == false){
                if(vol_err_cnt++ > 15){
                    vol_err_cnt = 0;
                    g_under_vol_flag = true;
                    log_e("under_vol: %d", l1_val);
                }
            }else if(l1_val > 513 && g_under_vol_flag == true){ // 当Urms<242时清除欠压标志
                if(vol_err_cnt++ > 15){
                    vol_err_cnt = 0;
                    g_under_vol_flag = false;
                    log_i("clear under_vol flag: %d", l1_val);
                }
            }

            if(pe_val > 350){
                if(g_pe_error_flag == false && pe_val <= 550){
                    if(pe_err_cnt++ > 2){
                        pe_err_cnt = 0;
                        g_pe_error_flag = true;
                        log_e("pe_error: %d", pe_val);
                    }
                }else if(g_pe_error_flag == true && pe_val >= 700){
                    if(pe_err_cnt++ > 2){
                        pe_err_cnt = 0;
                        g_pe_error_flag = false;
                        log_i("clear pe_error flag: %d", pe_val);
                    }
                }
            }else{
                if(g_pe_error_flag == false && pe_val >= 120){
                    if(pe_err_cnt++ > 2){
                        pe_err_cnt = 0;
                        g_pe_error_flag = true;
                        log_e("pe_error: %d", pe_val);
                    }
                }else if(g_pe_error_flag == true && pe_val <= 10){
                    if(pe_err_cnt++ > 2){
                        pe_err_cnt = 0;
                        g_pe_error_flag = false;
                        log_i("clear pe_error flag: %d", pe_val);
                    }
                }
            }

            max_cur = evse_get_max_current();
            if(cur > 1.15f*max_cur && g_over_cur_flag == false){// cur > 1.15*I_RMS 时触发过流报警(单位mA)
                if(cur_err_cnt++ > 2){
                    g_over_cur_flag = true;
                    log_e("cur error: %0.2f", cur);
                }
            }
            #if defined(CUR_ERR_CAN_BE_CLEAR)   // 判断过流报警是否可以被清除
            else if(cur < 1.1f*max_cur && g_over_cur_flag == true){// cur < 1.1*I_RMS 时恢复过流报警(单位mA)
                cur_err_cnt = 0;
                g_over_cur_flag = false;
                log_i("clear cur error: %0.2f", cur);
            }
            #endif
            // log_d("cur: %.3f, vol: %.3f, power: %0.3f", cur, vol, power);
            // log_i("pe_val: %d, l1_val: %d, c_val: %d", pe_val, l1_val, c_val);
            /* 重新开启中断 */
            exti_interrupt_flag_clear(EXTI_6);
            exti_interrupt_enable(EXTI_6);
        }
        if(second_flag){
            second_flag = false;
            // log_d("cur: %.3f, vol: %.3f, power: %0.3f", cur, vol, power);
            if(power < 50.0f){
                power = 0;
            }
            evse_ui_update(UI_CMD_UPDATE_POWER, NULL, &power);
        }
        bos_delay_ms(10);
    }
}
bos_task_export(voltage_sample, task_entry_voltage_sample, BOS_MAX_PRIORITY, NULL);

/**
 * @brief  计算kwh
 * @note   目前只是用来实时更新充电功率。
 */
static void task_entry_kwh_calc(void *parameter)
{
    /* 等待充电桩主任务完成初始化 */
    while (evse_get_state() == EVSE_REBOOT)
    {
        bos_delay_ms(10);
    }
    second_flag = false;
    for(;;){
        if(second_flag){
            second_flag = false;
            // if(cur > 0.2f)
            //     g_kwh += (float)((double)power/(double)3600000.0);
            // log_d("cur: %.3f, vol: %.3f, power: %0.3f, kwh: %0.4f", cur, vol, power, g_kwh);
            
            /* 功率小于50W时，显示0W */
            if(power < 50.0f){
                power = 0;
            }
            evse_ui_update(UI_CMD_UPDATE_POWER, NULL, &power);
            // log_d("power: %0.3f", power);
        }
        bos_delay_ms(100);
    }
}
// bos_task_export(kwh_calc, task_entry_kwh_calc, BOS_MAX_PRIORITY, NULL);

void evse_ac_init(void)
{
    cplt_flag = false;
    evse_ac_adc_config();
    freq_exti_config();
    evse_ac_timer_config(5000); // 5000Hz/50Hz = 100个
    exti_interrupt_enable(EXTI_6);
}

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

/* 用来标记一个正弦波的开始 */
static void freq_exti_config(void)
{
    /* enable the GPIO clock */
    rcu_periph_clock_enable(TRIG_PORT_RCU);
    rcu_periph_clock_enable(RCU_AF);
    /* configure GPIO pin as input */
    gpio_init(TRIG_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_MAX, TRIG_PIN);
    /* enable and set key EXTI interrupt to the lowest priority */
    nvic_irq_enable(EXTI5_9_IRQn, 5U, 0U);
    /* connect key EXTI line to key GPIO pin */
    gpio_exti_source_select(TRIG_SOURCE_PORT, TRIG_SOURCE_PIN);
    
    exti_interrupt_flag_clear(EXTI_6);
    /* configure key EXTI line */
    exti_init(EXTI_6, EXTI_INTERRUPT, EXTI_TRIG_RISING);
}

/**
 * @brief   timer1 pwm(TIMER1_CH1)初始化(用来触发adc采样)
 * @param   f 定时器更新频率, 单位Hz(2 - 1000000)
 * @note    TIMER1CLK(TIMER1_CK/PSC)固定为1000 000Hz(1MHz), 通过这个算出PSC寄存器的数值
*/
static void evse_ac_timer_config(uint16_t f)
{
    timer_parameter_struct timer_initpara;      // 定时器基本参数
    timer_oc_parameter_struct timer_ocintpara;  // 定时器输出设置

    rcu_periph_clock_enable(RCU_TIMER0);

    timer_deinit(TIMER0);
    /* TIMER configuration */
    timer_initpara.prescaler         = ((timer_source_clock_get(TIMER0)/1000000U)-1); // TIMER2CLK(TIMER2_CK/PSC) is 100KHz
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = (1000000U/f)-1;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(TIMER0,&timer_initpara);

    /* CH0 configuration in PWM mode0 */
    timer_ocintpara.ocpolarity  = TIMER_OC_POLARITY_HIGH;
    timer_ocintpara.outputstate = TIMER_CCX_ENABLE;
    timer_channel_output_config(TIMER0, TIMER_CH_0, &timer_ocintpara);

    timer_channel_output_pulse_value_config(TIMER0, TIMER_CH_0, 1);
    timer_channel_output_mode_config(TIMER0, TIMER_CH_0, TIMER_OC_MODE_PWM1);
    timer_channel_output_shadow_config(TIMER0, TIMER_CH_0, TIMER_OC_SHADOW_ENABLE);

    timer_update_event_enable(TIMER0);                                  // 配置TIMERx_CTL0的UPDIS
    timer_single_pulse_mode_config(TIMER0, TIMER_SP_MODE_REPETITIVE);   // 配置TIMERx_CTL0的SPM(配置为连续模式)
    timer_update_source_config(TIMER0, TIMER_UPDATE_SRC_REGULAR);       // 配置TIMERx_CTL0的UPS

    /* TIMER0 primary output enable */
    timer_primary_output_config(TIMER0, ENABLE);
    /* auto-reload preload enable */
    timer_auto_reload_shadow_enable(TIMER0); 

    /* auto-reload preload enable */
    timer_disable(TIMER0);
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
static void evse_ac_adc_config(void)
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
    adc_external_trigger_source_config(AC_ADC, ADC_REGULAR_CHANNEL, ADC0_1_EXTTRIG_REGULAR_T0_CH0);
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

/*!
    \brief      this function handles external lines 10 to 15 interrupt request
    \param[in]  none
    \param[out] none
    \retval     none
*/
void EXTI5_9_IRQHandler(void)
{
    if(RESET != (EXTI_PD & (uint32_t)EXTI_6)){   // 开启电压采集: 开启定时器
        timer_counter_value_config(TIMER0, 0xC7); // timer_counter_value_config(TIMER0, 0xC7);
        timer_enable(TIMER0); // timer_enable(TIMER0);
        EXTI_PD = (uint32_t)EXTI_6;         // exti_interrupt_flag_clear(EXTI_6);
    }
}

void DMA0_Channel0_IRQHandler(void)
{
    if(dma_interrupt_flag_get(DMA0, DMA_CH0, DMA_INT_FLAG_FTF)){    // 完成采样后, 先暂停采样
        DMA_INTC(DMA0) |= DMA_FLAG_ADD(DMA_INT_FLAG_FTF, DMA_CH0); // dma_interrupt_flag_clear(DMA0, DMA_CH0, DMA_INT_FLAG_FTF);
        TIMER_CTL0(TIMER0) &= ~(uint32_t)TIMER_CTL0_CEN;    // timer_disable(TIMER1);
        EXTI_INTEN &= ~(uint32_t)EXTI_6;    // exti_interrupt_disable(EXTI_6);

        cplt_flag = true;
    }
}

/**
 * @brief   获取正弦波正半波的平均值
 */
static uint16_t get_sin_val(__IO const uint16_t pBuff[][CH_NUM], uint16_t length, uint16_t index)
{
    static uint32_t val;
    static uint16_t base_val;
    uint16_t temp = 0;
    uint32_t sum = 0, count = 0;

    if(index > CH_NUM){
        log_e("index > CH_NUM");
        return 65535;
    }

    for(uint32_t i = 0; i < length; i++){
        sum += pBuff[i][index];
    }
    base_val = ((6*base_val) + (uint16_t)(4.0*(sum/length)))/10.0;
    // log_d("base_val: %d", base_val);

    for(uint32_t i = 0; i < length; i++){
        val += abs((int32_t)pBuff[i][index]-(int32_t)base_val);
        // log_d("val: %d", abs((int32_t)pBuff[i][index]-(int32_t)base_val));
    }

    // log_d("base_val: %d, val: %d", base_val, val);
    val /= length;

    return (uint16_t)val;
}
