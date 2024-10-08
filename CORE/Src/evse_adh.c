#include "evse_adh.h"
#include "basic_os.h"
#include "drv_timer.h"
#include "evse_ac.h"
#include "evse_relay.h"

#define LOG_TAG "evse.adc"
#include "elog.h"

/* Freq exti */
#define FREQ_IN_PORT        GPIOB
#define FREQ_IN_PORT_RCU    RCU_GPIOB
#define FREQ_IN_PIN         GPIO_PIN_6
/* 火线出线电压 */
#define VL_OUT_PORT     GPIOA
#define VL_OUT_PORT_RCU RCU_GPIOA
#define VL_OUT_PIN      GPIO_PIN_5

#define VL_OUT_ADC_CH   ADC_CHANNEL_5
/* 零线出线电压 */
#define VN_OUT_PORT     GPIOA
#define VN_OUT_PORT_RCU RCU_GPIOA
#define VN_OUT_PIN      GPIO_PIN_6

#define VN_OUT_ADC_CH   ADC_CHANNEL_6

#define CH_NUM              (4)
#define SAMPLE_NUM          (100)

__IO adh_state_t g_adh_state = ADH_OK;

/* ADC转换完成后，该指针指向存放ADC原始数据的DMA缓冲区 */
__IO uint16_t (*p_adh_adc_buff)[2] = NULL;

// adc 采样数据DMA缓冲区
__attribute((used)) uint16_t adh_adc_buff[100][2];

void evse_adh_exti_config(void)
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

/**
 * @brief   ADC DMA配置
*/
static void evse_adh_adc_dma_config(void)
{
    /* ADC_DMA_channel configuration */
    dma_parameter_struct dma_data_parameter;
    
    /* ADC_DMA_channel deinit */
    dma_deinit(DMA0, DMA_CH0);

    rcu_periph_clock_enable(RCU_DMA0);
    /* initialize DMA single data mode */
    dma_data_parameter.periph_addr  = (uint32_t)(&ADC_RDATA(ADC0));
    dma_data_parameter.periph_inc   = DMA_PERIPH_INCREASE_DISABLE;
    dma_data_parameter.memory_addr  = (uint32_t)(adh_adc_buff);
    dma_data_parameter.memory_inc   = DMA_MEMORY_INCREASE_ENABLE;
    dma_data_parameter.periph_width = DMA_PERIPHERAL_WIDTH_16BIT;
    dma_data_parameter.memory_width = DMA_MEMORY_WIDTH_16BIT;
    dma_data_parameter.direction    = DMA_PERIPHERAL_TO_MEMORY;
    dma_data_parameter.number       = 100*2;
    dma_data_parameter.priority     = DMA_PRIORITY_HIGH;
    dma_flag_clear(DMA0, DMA_CH0, DMA_FLAG_HTF|DMA_FLAG_FTF);
    dma_init(DMA0, DMA_CH0, &dma_data_parameter);
  
    dma_circulation_enable(DMA0, DMA_CH0);
    dma_interrupt_enable(DMA0,DMA_CH0, DMA_CHXCTL_FTFIE);
    nvic_irq_enable(DMA0_Channel0_IRQn,5,0);
    /* enable DMA channel */
    dma_channel_enable(DMA0, DMA_CH0);
}

/**
 * @brief   粘连检测
*/
void evse_adh_adc_config(void)
{
    adc_deinit(ADC0);
    /* GPIO配置 */
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_init(VL_OUT_PORT, GPIO_MODE_AIN, GPIO_OSPEED_MAX, VL_OUT_PIN);
    gpio_init(VN_OUT_PORT, GPIO_MODE_AIN, GPIO_OSPEED_MAX, VN_OUT_PIN);
    /* enable ADC clock */
    rcu_periph_clock_enable(RCU_ADC0);
    /* config ADC clock */
    rcu_adc_clock_config(RCU_CKADC_CKAPB2_DIV2);
    /* ADC mode config */
    adc_mode_config(ADC_MODE_FREE);
    /* ADC data alignment config */
    adc_data_alignment_config(ADC0, ADC_DATAALIGN_RIGHT);
    /* ADC SCAN function disable */
    adc_special_function_config(ADC0, ADC_SCAN_MODE, ENABLE);
    /* 关闭连续模式(触发一次转换一次) */
    adc_special_function_config(ADC0, ADC_CONTINUOUS_MODE, DISABLE);
    /* ADC channel length config */
    adc_channel_length_config(ADC0, ADC_REGULAR_CHANNEL, 2);
    /* ADC0_CHx config */
    adc_regular_channel_config(ADC0, 0, VL_OUT_ADC_CH, ADC_SAMPLETIME_7POINT5);
    adc_regular_channel_config(ADC0, 1, VN_OUT_ADC_CH, ADC_SAMPLETIME_7POINT5);
    /* ADC trigger config */
    adc_external_trigger_source_config(ADC0, ADC_REGULAR_CHANNEL, ADC0_1_EXTTRIG_REGULAR_T2_TRGO); // 由TIMER2_TRGO触发
    /* ADC external trigger enable */
    adc_external_trigger_config(ADC0, ADC_REGULAR_CHANNEL, ENABLE);   // 软件触发也算外部触发的。
    /* enable ADC interface */
    adc_enable(ADC0);
    
    bos_delay_ms(1); // 延时一下

    /* ADC calibration and reset calibration */
    adc_calibration_enable(ADC0);

    adc_dma_mode_enable(ADC0);

    /* 放在这里初始化, 用来等待ADC完成启动 */
    evse_adh_adc_dma_config();      // 配置DMA
}

/**
 * @brief   CP PWM 输出初始化, 默认输出低电平
 * @param   f PWM频率, 单位Hz(2 - 1000000)
 * @note    TIMERxCLK(TIMERx_CK/PSC)固定为1000 000Hz(1MHz), 通过这个算出PSC寄存器的数值
*/
void evse_adh_trig_config(uint32_t f)
{
    timer_parameter_struct timer_initpara;      // 定时器基本参数
    timer_oc_parameter_struct timer_ocintpara;  // 定时器输出设置

    rcu_periph_clock_enable(RCU_TIMER2);
    timer_deinit(TIMER2);
    /* TIMER configuration */
    timer_initpara.prescaler         = ((timer_source_clock_get(TIMER2)/1000000U)-1); // 预分频后是 1MHz
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = (1000000U/f)-1;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(TIMER2, &timer_initpara);

    timer_update_event_enable(TIMER2);                                  // 配置TIMERx_CTL0的UPDIS
    timer_single_pulse_mode_config(TIMER2, TIMER_SP_MODE_REPETITIVE);   // 配置TIMERx_CTL0的SPM(配置为连续模式)
    timer_update_source_config(TIMER2, TIMER_UPDATE_SRC_GLOBAL);        // 配置TIMERx_CTL0的UPS

    timer_primary_output_config(TIMER2, ENABLE);
    /* auto-reload preload enable */
    timer_auto_reload_shadow_enable(TIMER2);

    timer_enable(TIMER2);
}

// void EXTI5_9_IRQHandler(void)
// {
//     if(RESET != exti_interrupt_flag_get(EXTI_6)) {
//         // exti6_flag = true;
//         exti_interrupt_disable(EXTI_6);
//         dma_transfer_number_config(DMA0, DMA_CH0, 200);  // DMA重新开始计数
//         timer_counter_value_config(TIMER1, 0);
//         timer_enable(TIMER1);
//     }
//     exti_interrupt_flag_clear(EXTI_6);
// }

// void DMA0_Channel0_IRQHandler(void)
// {
//     if(dma_interrupt_flag_get(DMA0, DMA_CH0, DMA_INT_FLAG_FTF)){
//         dma_interrupt_flag_clear(DMA0, DMA_CH0, DMA_INT_FLAG_FTF);
//         p_adh_adc_buff = adh_adc_buff;
//         // exti_interrupt_enable(EXTI_6);
//         timer_disable(TIMER2);
//     }
// }

static void task_entry_adh_check(void *parameter)
{
    uint16_t adh_l = 0, adh_n = 0;
    evse_adh_adc_config();
    evse_adh_trig_config(5000);
    
    evse_relay_init();
    evse_relay_ctrl(open);

    bos_delay_ms(500);
    evse_relay_ctrl(close);

    TIMER_CTL1(TIMER2) &= (~(uint32_t)TIMER_CTL1_MMC);
    TIMER_CTL1(TIMER2) |= (uint32_t)TIMER_TRI_OUT_SRC_UPDATE; // 由更新事件产生TRGO信号

    for(;;){
        if(p_adh_adc_buff != NULL) {
            adh_l = get_sin_vol(p_adh_adc_buff, 100, 0);
            adh_n = get_sin_vol(p_adh_adc_buff, 100, 1);
            
            if(adh_l > 10 || adh_n > 10) g_adh_state = ADH_ERROR;
            else g_adh_state = ADH_OK;

            log_d("adh_l: %d, adh_n: %d", adh_l, adh_n);
            p_adh_adc_buff = NULL;
            
            // 处理完后重新打开定时器
            timer_counter_value_config(TIMER2, 0);
            dma_transfer_number_config(DMA0, DMA_CH0, 200);
            timer_enable(TIMER2);
        }
        bos_delay_ms(1);
    }
}
// bos_task_export(adh_check, task_entry_adh_check, BOS_MAX_PRIORITY, NULL);
