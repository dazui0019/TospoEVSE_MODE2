#include "evse_cp.h"
#include "drv_delay.h"
#include "EventRecorder.h"
#include "basic_os.h"
#include "evse_relay.h"
#include "evse_charge.h"
#include "evse_gndd.h"

#define LOG_TAG "evse.cp"
#include "elog.h"

#define SAMPLE_NUM      (10)    // 采样次数(SAMPLE_NUM个CP周期)
#define ADC_TH          (0x30)  // ADC限幅滤波阈值

/* CP输出 */
#define CP_TIMER        (TIMER1)
#define CP_TIMER_RCU    (RCU_TIMER1)
#define CP_TIMER_CH     (TIMER_CH_1)
#define CP_TIMER_IRQ    (TIMER1_IRQn)
#define CP_PORT         (GPIOA)
#define CP_PIN          (GPIO_PIN_1)
#define CP_PORT_RCU     (RCU_GPIOA)
#define CP_PWM_MODE     (TIMER_OC_MODE_PWM1)
#define CK_ADC          ADC2
#define CK_ADC_RCU      RCU_ADC2
/* CP检测 */
#define CK_CP_PORT      GPIOA
#define CK_CP_RCU       RCU_GPIOA
#define CK_CP_PIN       GPIO_PIN_0

#define CK_CP_ADC_CH    ADC_CHANNEL_0

// 定义CP电平阈值
// #define CP_12V_TH   3500
// #define CP_9V_TH    2590
// #define CP_6V_TH    1680
// #define CP_OFFSET   100
#define CP_12V_TH   3.0f
#define CP_9V_TH    2.2f
#define CP_6V_TH    1.4f
#define CP_OFFSET   0.2f

// 定义CP电压状态
#define STATE_CP_12V    (1<<0)
#define STATE_CP_9V     (1<<1)
#define STATE_CP_6V     (1<<2)
#define STATE_CP_3V     (1<<3)
#define STATE_CP_UNK    (1<<4)  // 未知状态
#define STATE_CP_ERROR  (1<<5)

extern __IO uint16_t g_Vrefint;  // 芯片内部1.2V参考电压的 ADC 原始值
uint16_t gnd_base = 0;

// 占空比表， 是CAR寄存器的数值, 表示最大电流值所对应的CP波形占空比数值
// 其中第一个元素(50), 表示占空比为5%需要进行数字通信而不是电流
const static uint16_t duty_table[] = {
     50, 100, 100, 100, 100, 100, 100, 117, 133, 150,
    167, 183, 200, 217, 233, 250, 267, 283, 300, 317,
    333, 350, 367, 383, 400, 417, 433, 450, 467, 483,
    500, 517, 533, 550, 567, 583, 600, 617, 633, 650,
    667, 683, 700, 717, 733, 750, 767, 783, 800, 817,
    833, 850, 850, 852, 856, 860, 864, 868, 872, 876,
    880, 884, 888, 892, 896, 900
};

cp_t g_cp = {
    .pwm_state  = DISABLE,
    .ck_state   = CK_OFF,
    .state      = CP_INIT,
    .init       = cp_pwm_init,
    .set_cur    = cp_cur_set,
    .pwm_ctrl   = pwm_ctrl,
    .ck_ctrl    = ck_ctrl,
    .get_cp_vol = get_voltage,
    .get_cp_state = get_cp_state
};

/* ADC转换完成后，该指针指向存放ADC原始数据的DMA缓冲区 */
__IO uint16_t *g_p_cp_buff = NULL;

// adc 采样数据DMA缓冲区
__attribute((used)) uint16_t cp_buff[2][10];

/**
 * @brief   CP PWM 输出初始化, 默认输出低电平
 * @param   f PWM频率, 单位Hz(2 - 1000000)
 * @note    TIMERxCLK(TIMERx_CK/PSC)固定为1000 000Hz(1MHz), 通过这个算出PSC寄存器的数值
*/
void cp_pwm_init(uint32_t f)
{
    timer_parameter_struct timer_initpara;      // 定时器基本参数
    timer_oc_parameter_struct timer_ocintpara;  // 定时器输出设置

    /*Configure PIN as remap function*/
    rcu_periph_clock_enable(CP_PORT_RCU);
    rcu_periph_clock_enable(RCU_AF);
    gpio_pin_remap_config(GPIO_TIMER2_PARTIAL_REMAP, ENABLE);       // 开启TIMER2的REMAP
    gpio_init(CP_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, CP_PIN); // 配置GPIO AF功能

    rcu_periph_clock_enable(CP_TIMER_RCU);
    timer_deinit(CP_TIMER);
    /* TIMER configuration */
    timer_initpara.prescaler         = ((timer_source_clock_get(CP_TIMER)/1000000U)-1); // 预分频后是 1MHz
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = (1000000U/f)-1;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(CP_TIMER, &timer_initpara);

    /* configurate CHx in PWM modex */
    timer_ocintpara.ocpolarity  = TIMER_OC_POLARITY_HIGH;
    timer_ocintpara.outputstate = TIMER_CCX_ENABLE;
    timer_channel_output_config(CP_TIMER, CP_TIMER_CH, &timer_ocintpara);

    timer_update_event_enable(CP_TIMER);                                  // 配置TIMERx_CTL0的UPDIS
    timer_single_pulse_mode_config(CP_TIMER, TIMER_SP_MODE_REPETITIVE);   // 配置TIMERx_CTL0的SPM(配置为连续模式)
    timer_update_source_config(CP_TIMER, TIMER_UPDATE_SRC_GLOBAL);        // 配置TIMERx_CTL0的UPS

    /* TIMERx channelx duty cycle = (((TIMER_CAR(CP_TIMER)+1)/20)/ TIMER_CAR(CP_TIMER))* 100  = 5% */    
    timer_channel_output_mode_config(CP_TIMER, CP_TIMER_CH, CP_PWM_MODE);        // 先输出低电平
    timer_channel_output_pulse_value_config(CP_TIMER, CP_TIMER_CH, 950); //
    timer_channel_output_shadow_config(CP_TIMER, CP_TIMER_CH, TIMER_OC_SHADOW_ENABLE);  // 使能CHxCV寄存器的影子寄存器

    timer_primary_output_config(CP_TIMER, DISABLE);
    /* auto-reload preload enable */
    timer_auto_reload_shadow_enable(CP_TIMER);

    timer_interrupt_flag_clear(CP_TIMER, TIMER_INT_UP);
    timer_interrupt_disable(CP_TIMER, TIMER_INT_UP);
    nvic_irq_enable(CP_TIMER_IRQ, 5, 0);

    timer_enable(CP_TIMER);

    /* CP电平检测初始化 */
    cp_check_init();
}

/**
 * @brief   在定时器中断中，软件触发ADC转换
 */
void TIMER2_IRQHandler(void)
{
    /* TIMER2只开了一个中断，所以就不判断了 */
    ADC_CTL1(CK_ADC) |= ADC_CTL1_SWRCST;    // 软件触发ADC转换
    TIMER_INTF(TIMER2) = (~(uint32_t)TIMER_INT_UP); // 清除中断标志位
    // if(timer_interrupt_flag_get(TIMER2, TIMER_INT_UP) != RESET){
    //     timer_interrupt_flag_clear(TIMER2, TIMER_INT_UP);
    //     adc_software_trigger_enable(CK_ADC, ADC_REGULAR_CHANNEL);
    //     ADC_CTL1(CK_ADC) |= ADC_CTL1_SWRCST;
    // }
}

/**
 * @brief   在定时器中断中，软件触发ADC转换
 */
void TIMER1_IRQHandler(void)
{
    /* TIMER1只开了一个中断，所以就不判断了 */
    ADC_CTL1(CK_ADC) |= ADC_CTL1_SWRCST;    // 软件触发ADC转换
    TIMER_INTF(TIMER1) = (~(uint32_t)TIMER_INT_UP); // 清除中断标志位
    // if(timer_interrupt_flag_get(TIMER2, TIMER_INT_UP) != RESET){
    //     timer_interrupt_flag_clear(TIMER2, TIMER_INT_UP);
    //     adc_software_trigger_enable(CK_ADC, ADC_REGULAR_CHANNEL);
    //     ADC_CTL1(CK_ADC) |= ADC_CTL1_SWRCST;
    // }
}

/**
 * @brief   ADC DMA配置
*/
static void cp_check_dma_config(void)
{
    /* ADC_DMA_channel configuration */
    dma_parameter_struct dma_data_parameter;
    
    /* ADC_DMA_channel deinit */
    dma_deinit(DMA1, DMA_CH4);

    rcu_periph_clock_enable(RCU_DMA1);
    /* initialize DMA single data mode */
    dma_data_parameter.periph_addr  = (uint32_t)(&ADC_RDATA(ADC2));
    dma_data_parameter.periph_inc   = DMA_PERIPH_INCREASE_DISABLE;
    dma_data_parameter.memory_addr  = (uint32_t)(cp_buff);
    dma_data_parameter.memory_inc   = DMA_MEMORY_INCREASE_ENABLE;
    dma_data_parameter.periph_width = DMA_PERIPHERAL_WIDTH_16BIT;
    dma_data_parameter.memory_width = DMA_MEMORY_WIDTH_16BIT;
    dma_data_parameter.direction    = DMA_PERIPHERAL_TO_MEMORY;
    dma_data_parameter.number       = 10*2;
    dma_data_parameter.priority     = DMA_PRIORITY_HIGH;
    dma_flag_clear(DMA1, DMA_CH4, DMA_FLAG_HTF|DMA_FLAG_FTF);
    dma_init(DMA1, DMA_CH4, &dma_data_parameter);
  
    dma_circulation_enable(DMA1, DMA_CH4);
    dma_interrupt_enable(DMA1,DMA_CH4, DMA_FLAG_HTF|DMA_CHXCTL_FTFIE);
    nvic_irq_enable(DMA1_Channel3_Channel4_IRQn,5,0);
    /* enable DMA channel */
    dma_channel_enable(DMA1, DMA_CH4);
}

/**
 * @brief   CP和接地检测初始化
*/
void cp_check_init(void)
{
    adc_deinit(CK_ADC);
    /* GPIO配置 */
    rcu_periph_clock_enable(CK_CP_RCU);
    gpio_init(CK_CP_PORT, GPIO_MODE_AIN, GPIO_OSPEED_MAX, CK_CP_PIN);
    /* enable ADC clock */
    rcu_periph_clock_enable(CK_ADC_RCU);
    /* config ADC clock */
    rcu_adc_clock_config(RCU_CKADC_CKAPB2_DIV2);
    /* ADC mode config */
    adc_mode_config(ADC_MODE_FREE);
    /* ADC data alignment config */
    adc_data_alignment_config(CK_ADC, ADC_DATAALIGN_RIGHT);
    /* ADC SCAN function disable */
    adc_special_function_config(CK_ADC, ADC_SCAN_MODE, DISABLE);
    /* 关闭连续模式(触发一次转换一次) */
    adc_special_function_config(CK_ADC, ADC_CONTINUOUS_MODE, DISABLE);
    /* ADC channel length config */
    adc_channel_length_config(CK_ADC, ADC_REGULAR_CHANNEL, 1);
    /* ADC0_CHx config */
    adc_regular_channel_config(CK_ADC, 0, CK_CP_ADC_CH, ADC_SAMPLETIME_7POINT5);
    /* ADC trigger config */
    adc_external_trigger_source_config(CK_ADC, ADC_REGULAR_CHANNEL, ADC0_1_2_EXTTRIG_REGULAR_NONE); // 由软件触发
    /* ADC external trigger enable */
    adc_external_trigger_config(CK_ADC, ADC_REGULAR_CHANNEL, ENABLE);   // 软件触发也算外部触发的。
    /* enable ADC interface */
    adc_enable(CK_ADC);
    
    bos_delay_ms(1); // 延时一下

    /* ADC calibration and reset calibration */
    adc_calibration_enable(CK_ADC);

    adc_dma_mode_enable(CK_ADC);

    /* 放在这里初始化, 用来等待ADC完成启动 */
    cp_check_dma_config();      // 配置DMA
}

/**
 * @brief   DMA中断处理函数
 */
void DMA1_Channel3_4_IRQHandler(void)
{
    if(dma_interrupt_flag_get(DMA1, DMA_CH4, DMA_INT_FLAG_FTF)){
        dma_interrupt_flag_clear(DMA1, DMA_CH4, DMA_INT_FLAG_FTF);
        g_p_cp_buff = cp_buff[1];
    }else if(dma_interrupt_flag_get(DMA1, DMA_CH4, DMA_INT_FLAG_HTF)){
        dma_interrupt_flag_clear(DMA1, DMA_CH4, DMA_INT_FLAG_HTF);
        g_p_cp_buff = cp_buff[0];
    }
}

/**
 * @brief   PWM输出控制
 * @param   status:
 *              ENABLE  开启PWM输出
 *              DISABLE 关闭PWM输出
 */
void pwm_ctrl(ControlStatus status){
    switch(status){
    case ENABLE:
        cp_enable();
        break;
    case DISABLE:
        cp_disable();
        break;
    default:
        break;
    }
}

/**
 * @brief   开启PWM输出
 * @note    参考timer_channel_output_mode_config()函数
*/
void cp_enable(void)
{
    switch(CP_TIMER_CH){
    /* configure TIMER_CH_0 */
    case TIMER_CH_0:
        TIMER_CHCTL0(CP_TIMER) &= (~(uint32_t)TIMER_CHCTL0_CH0COMCTL);
        TIMER_CHCTL0(CP_TIMER) |= (uint32_t)CP_PWM_MODE;
        break;
    /* configure TIMER_CH_1 */
    case TIMER_CH_1:
        TIMER_CHCTL0(CP_TIMER) &= (~(uint32_t)TIMER_CHCTL0_CH1COMCTL);
        TIMER_CHCTL0(CP_TIMER) |= (uint32_t)((uint32_t)(CP_PWM_MODE) << 8U);
        break;
    /* configure TIMER_CH_2 */
    case TIMER_CH_2:
        TIMER_CHCTL1(CP_TIMER) &= (~(uint32_t)TIMER_CHCTL1_CH2COMCTL);
        TIMER_CHCTL1(CP_TIMER) |= (uint32_t)CP_PWM_MODE;
        break;
    /* configure TIMER_CH_3 */
    case TIMER_CH_3:
        TIMER_CHCTL1(CP_TIMER) &= (~(uint32_t)TIMER_CHCTL1_CH3COMCTL);
        TIMER_CHCTL1(CP_TIMER) |= (uint32_t)((uint32_t)(CP_PWM_MODE) << 8U);
        break;
    default:
        break;
    }
}

/**
 * @brief   关闭PWM输出(强制输出高电平)
 * @note    参考timer_channel_output_mode_config()函数
*/
void cp_disable(void)
{
    switch(CP_TIMER_CH){
    /* configure TIMER_CH_0 */
    case TIMER_CH_0:
        TIMER_CHCTL0(CP_TIMER) &= (~(uint32_t)TIMER_CHCTL0_CH0COMCTL);
        TIMER_CHCTL0(CP_TIMER) |= (uint32_t)TIMER_OC_MODE_HIGH;
        break;
    /* configure TIMER_CH_1 */
    case TIMER_CH_1:
        TIMER_CHCTL0(CP_TIMER) &= (~(uint32_t)TIMER_CHCTL0_CH1COMCTL);
        TIMER_CHCTL0(CP_TIMER) |= (uint32_t)((uint32_t)(TIMER_OC_MODE_HIGH) << 8U);
        break;
    /* configure TIMER_CH_2 */
    case TIMER_CH_2:
        TIMER_CHCTL1(CP_TIMER) &= (~(uint32_t)TIMER_CHCTL1_CH2COMCTL);
        TIMER_CHCTL1(CP_TIMER) |= (uint32_t)TIMER_OC_MODE_HIGH;
        break;
    /* configure TIMER_CH_3 */
    case TIMER_CH_3:
        TIMER_CHCTL1(CP_TIMER) &= (~(uint32_t)TIMER_CHCTL1_CH3COMCTL);
        TIMER_CHCTL1(CP_TIMER) |= (uint32_t)((uint32_t)(TIMER_OC_MODE_HIGH) << 8U);
        break;
    default:
        break;
    }
}

/**
 * @brief      设置充电电流最大值(修改PWM占空比)
 * @param[in]  cur: 1 ~ 63
*/
uint8_t cp_cur_set(uint8_t cur)
{
    // uint16_t pwm_flag;
    /* duty(%) =  ((TIMER_CAR(CP_TIMER) + 1)/TIMER_CH0CV(CP_TIMER)) * 100 */
    if(cur < 1 || cur > 63){
        log_e("param error.");
        return 1;
    }
    g_cp.current = cur; // 更新电流大小

    // if(CP_TIMER_CH == TIMER_CH_0){
    //     pwm_flag = (uint16_t)((TIMER_CHCTL0(CP_TIMER)) & ((uint32_t)TIMER_CHCTL0_CH0COMCTL));
    // }else if(CP_TIMER_CH == TIMER_CH_1){
    //     pwm_flag = (uint16_t)((TIMER_CHCTL0(CP_TIMER)) & ((uint32_t)TIMER_CHCTL0_CH1COMCTL));
    //     pwm_flag >>= 8;
    // }else{
    //     log_e("timer_ch_%d is not supported", CP_TIMER_CH);
    //     return 1;
    // }

    // if(TIMER_OC_MODE_PWM0 == pwm_flag){
    //     timer_channel_output_pulse_value_config(CP_TIMER, CP_TIMER_CH, duty_table[cur]);
    //     log_d("PWM0");
    // }
    // else if (TIMER_OC_MODE_PWM1 == (pwm_flag)){
    //     timer_channel_output_pulse_value_config(CP_TIMER, CP_TIMER_CH, 1000-duty_table[cur]);
    //     log_d("PWM1");
    // }else{
    //     log_e("CH%dCOMCTL[2:0]: 0x%X", CP_TIMER_CH, pwm_flag);
    // }

    if(CP_PWM_MODE == TIMER_OC_MODE_PWM0){
        timer_channel_output_pulse_value_config(CP_TIMER, CP_TIMER_CH, duty_table[cur]);
    }else if(CP_PWM_MODE == TIMER_OC_MODE_PWM1){
        timer_channel_output_pulse_value_config(CP_TIMER, CP_TIMER_CH, 1000-duty_table[cur]);
    }else{
        log_e("unknown PWM mode: %d", CP_PWM_MODE);
    }

    return 0;
}

/**
 * @brief   CP电压检测控制
 * @param   status:
 *              ENABLE  开启电压检测
 *              DISABLE 关闭电压检测
 */
void ck_ctrl(ControlStatus status){
    switch(status){
    case ENABLE:
        timer_interrupt_enable(CP_TIMER, TIMER_INT_UP); // 开启更新中断
        break;
    case DISABLE:
        timer_interrupt_disable(CP_TIMER, TIMER_INT_UP); // 关闭更新中断
        break;
    default:
        break;
    }
}

uint16_t get_cp_vol(__IO uint16_t pBuff[][2], uint16_t length)
{
    static uint16_t voltage;
    uint16_t temp = 0;
    uint32_t sum = 0, count = 0;
    uint16_t temp_buff[SAMPLE_NUM] = {0};

    /* 检测CP */
    /**
     * 在采样数据中找到第一个大于100的数(正常的ADC_RAW值会大于2000)，赋值给temp，作为滤波器的初始值。
     * 这一步是为了防止temp设置过小，导致滤波结果不准确。
    */
    for(uint32_t i = 0; i < (length-1); i++){
        if(abs(pBuff[i][0] - pBuff[i+1][0]) <= ADC_TH){
            temp = pBuff[i][0];
            if (temp > 100){
                break;
            }
        }
    }

    for(uint32_t i = 0; i < length; i++){
        if(abs((int32_t)(pBuff[i][0] - temp)) <= ADC_TH){
            sum += pBuff[i][0];
            temp = pBuff[i][0];
            temp_buff[i] = pBuff[i][0];
            count++;
        }
    }

    if(count >= (length/2))  // 如果求和数量太少的话(小于一半), 本次计算出来的数据可信度就会比较低，因此直接忽略
        voltage = (uint16_t)(sum/count);

    return voltage;
}

uint16_t get_gnd_vol(__IO uint16_t pBuff[][2], uint16_t length)
{
    static uint16_t voltage;
    uint16_t base_voltage;
    uint16_t temp = 0;
    uint32_t sum = 0, count = 0;

    for(uint32_t i = 0; i < length; i++){
        sum += pBuff[i][1];
    }
    base_voltage = (uint16_t)(sum/length);

    for(uint32_t i = 0; i < length; i++){
        voltage += abs((int32_t)pBuff[i][1]-(int32_t)base_voltage);
    }
    voltage /= length;

    return voltage;
}

/**
 * @brief   获取CP采样电压值
 * @param[in]  pBuff: 电压采样缓冲区
 * @return  滤波后的电压值数组
 * @note    滤波算法暂定为限幅平均滤波
*/
uint16_t get_voltage(__IO uint16_t pBuff[], uint16_t length)
{
    static uint16_t voltage;
    uint16_t temp = 0;
    uint32_t sum = 0, count = 0;
    uint16_t temp_buff[SAMPLE_NUM] = {0};

    sum = count = 0;
    
    /* 检测CP */
    /**
     * 在采样数据中找到第一个大于100的数(正常的ADC_RAW值会大于2000)，赋值给temp，作为滤波器的初始值。
     * 这一步是为了防止temp设置过小，导致滤波结果不准确。
    */
    for(uint32_t i = 0; i < (length-1); i++){
        if(abs(pBuff[i] - pBuff[i+1]) <= ADC_TH){
            temp = pBuff[i];
            if (temp > 100){
                break;
            }
        }
    }
    
    for(uint32_t i = 0; i < length; i++){
        if(abs((int32_t)(pBuff[i] - temp)) <= ADC_TH){
            sum += pBuff[i];
            temp = pBuff[i];
            temp_buff[i] = pBuff[i];
            count++;
        }
    }
    
    if(count >= (length/2))  // 如果求和数量太少的话(小于一半), 本次计算出来的数据可信度就会比较低，因此直接忽略
        voltage = (uint16_t)(sum/count);

    // if(voltage > 4000)
    //     for(int i = 0; i < length; i++){
    //             osPrintf("%u ", temp_buff[i]);
    //     }
    // osPrintf("\r\n");
    // osPrintf("voltage: %u, sum: %d, count: %u", voltage, sum, count);
    // osPrintf("\r\n");

    return voltage;
}

cp_state_t get_cp_state(float vol)
{
    // float cp_vol = (1.2f * (float)vol)/g_Vrefint;
    if((CP_12V_TH-CP_OFFSET < vol) && (vol < CP_12V_TH+CP_OFFSET))      {return CP_12V;}
    else if((CP_9V_TH-CP_OFFSET < vol) && (vol < CP_9V_TH+CP_OFFSET))   {return CP_9V;} // 检测到插枪
    else if((CP_6V_TH-CP_OFFSET < vol) && (vol < CP_6V_TH+CP_OFFSET))   {return CP_6V;} // 检测到插枪并且S2闭合
    else {
        // log_e("cp vol_err: %f", vol);
        return CP_ERROR;
    }
}

/* 快速排序算法 ------------------------ */
int32_t abs(int32_t x) {
    int32_t y = x >> 31;
    return (x ^ y) - y;
}

void swap(uint16_t* a, uint16_t* b) {
    uint16_t t = *a;
    *a = *b;
    *b = t;
}

int partition (uint16_t arr[], int low, int high) {
    uint16_t pivot = arr[high]; 
    int i = (low - 1); 

    for (int j = low; j <= high - 1; j++) {
        if (arr[j] < pivot) {
            i++; 
            swap(&arr[i], &arr[j]);
        }
    }
    swap(&arr[i + 1], &arr[high]);
    return (i + 1);
}

void quickSort(uint16_t arr[], int low, int high) {
    if (low < high) {
        uint16_t pi = partition(arr, low, high);
        quickSort(arr, low, pi - 1);
        quickSort(arr, pi + 1, high);
    }
}

void task_entry_cp_test(void *parameter)
{
    uint16_t vol;
    // cp_pwm_init(1000);
    // cp_cur_set(16);
    // pwm_ctrl(ENABLE);
    // timer_interrupt_enable(TIMER2, TIMER_INT_UP);
    /* 初始化CP */
    g_cp.init(1000);    // CP输出和检测初始化
    g_cp.set_cur(17);   // 设置最大电流
    g_cp.pwm_ctrl(ENABLE);
    g_cp.ck_ctrl(ENABLE);
    for(;;){
        if(g_p_cp_buff != NULL){
            vol = get_voltage(g_p_cp_buff, 10);
            log_i("vol: %d", vol);
            g_p_cp_buff = NULL;
        }
        bos_delay_ms(1);
    }
}
// bos_task_export(evse_cp, task_entry_cp_test, BOS_MAX_PRIORITY, NULL);