/**
 * @file        evse_charge.c
 * @brief       充电桩主任务void task_entry_evse_main(void *parameter)
 */
#include "evse_charge.h"
#include "basic_os.h"
#include "evse_relay.h"
#include "evse_gndd.h"
#include "evse_adh.h"
#include "evse_ac.h"

#define LOG_TAG "evse.evse"
#include "elog.h"

// 车端二极管检测
#define S1_CK_PORT     GPIOB
#define S1_CK_RCU      RCU_GPIOB
#define S1_CK_PIN      GPIO_PIN_15

void s1_ck_init(void);

extern __IO gndd_state_t g_gndd_state;
extern __IO adh_state_t g_adh_state;
extern __IO uint16_t (*g_p_adc2_buff)[2];
extern __IO uint16_t *g_p_cp_buff;
extern cp_t g_cp;
extern __IO uint16_t g_Vrefint;  // evse_ac

/* 错误标志位 */
extern __IO uint8_t g_overheat_flag;    // evse_ntc
extern __IO uint8_t  g_vol_error_flag;  // evse_ac
extern __IO uint8_t g_pe_error_flag;           // evse_ac

evse_t evse_mode2 = {
    .inited = false,
    .evse_relay_ctrl = evse_relay_ctrl,
    .evse_state = EVSE_REBOOT,
    .relay_state = open,
    .p_cp = &g_cp
};

evse_state_t (*state_func[])(cp_state_t) = {
    evse_idle_handle, evse_idle_handle, evse_9v_handle, evse_6v_handle,
    evse_sim_6v_handle, evse_charging_handle, evse_done_handle, evse_cp_lost_handle,
    evse_cp_error_handle,
};

void evse_init()
{
    ;
}

ErrStatus evse_error_ck(void)
{
    uint8_t error_flag = false;

    for(;;){
        if(g_overheat_flag == true){
            error_flag = true;
        }

        if(g_vol_error_flag == true){
            error_flag = true;
        }

        if(g_pe_error_flag == true){
            error_flag = true;
        }
        
        if(error_flag == false){
            break;
        }else{
            return ERROR;
        }
    }
    return SUCCESS;
}

static void task_entry_evse_main(void *parameter)
{
    uint16_t cp_val, gnd_val;                       // CP电平和接地检测的ADC Raw值。
    uint16_t adh_l = 0, adh_n = 0;

    uint8_t cp_state_cnt = 0;
    cp_state_t lase_cp_state = CP_INIT;
    
    /* 错误次数累计 */
    uint8_t gndd_error_cnt = 0;
    uint8_t cp_error_cnt = 0;

    evse_state_t last_evse_state, evse_state;
    last_evse_state = evse_state = EVSE_IDLE;
    
    /* 初始化继电器 */
    evse_relay_init();
    evse_relay_ctrl(open);
    
    /* 车端二极管检测 */
    s1_ck_init();

    /* 获取内部1.2V基准电压的ADC值 */
    adc_verf_config();
    bos_delay_ms(500);
    for(int i = 10; i>0; i--){
        adc_software_trigger_enable(ADC0, ADC_INSERTED_CHANNEL);
        while(adc_flag_get(ADC0, ADC_FLAG_EOIC) == RESET){}
        adc_flag_clear(ADC0, ADC_FLAG_EOIC);
        // log_i("Vrefint: %d", ADC_IDATA0(ADC0));
        g_Vrefint += ADC_IDATA0(ADC0);
    }
    g_Vrefint /= 10;
    log_d("Vrefint: %d", g_Vrefint);

    /* 初始化CP */
    g_cp.init(1000);    // CP输出和检测初始化
    g_cp.set_cur(6);    // 设置最大电流
    g_cp.pwm_ctrl(DISABLE);
    g_cp.ck_ctrl(ENABLE);

    log_d("CP init done.");

    log_i("EVSE_REBOOT.");

    for(;;){
        // 先检测一下错误标志
        if(g_p_cp_buff != NULL){
            evse_mode2.p_cp->state = evse_mode2.p_cp->get_cp_state(evse_mode2.p_cp->get_cp_vol(g_p_cp_buff, 10));
            g_p_cp_buff = NULL;
            if(SUCCESS == evse_error_ck()){
                evse_state = state_func[evse_state](evse_mode2.p_cp->state);
            }else{
                evse_fault_handle(evse_mode2.p_cp->state);  // 检测到错误时, 直接调用错误处理函数(不会修改变量evse_state)
            }
        }
        bos_delay_ms(1);
    }
}
bos_task_export(evse_main, task_entry_evse_main, BOS_MAX_PRIORITY, NULL);

/**
 * @brief   检查车端二极管S1是否存在
 */
void s1_ck_init(void)
{
    rcu_periph_clock_enable(S1_CK_RCU);
    gpio_init(S1_CK_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_MAX, S1_CK_PIN);
    basic_timer5_init(50000); // 50ms
}

ErrStatus evse_s1_ck(void)
{
    FlagStatus s1_state = gpio_input_bit_get(S1_CK_PORT, S1_CK_PIN);
    uint8_t cnt = 0;
    timer_counter_value_config(TIMER5, 0);
    timer_flag_clear(TIMER5, TIMER_FLAG_UP);
    
    timer_enable(TIMER5);
    while(RESET == timer_flag_get(TIMER5, TIMER_FLAG_UP)){
        if(s1_state != gpio_input_bit_get(S1_CK_PORT, S1_CK_PIN)){
            return SUCCESS;
        }
    }
    return ERROR;
}

evse_state_t evse_idle_handle(cp_state_t cp_state)
{
    if(evse_mode2.evse_state == EVSE_FAULT){    // 从错误中恢复时需要的处理
        log_i("Return frome EVSE_FAULT.");
    }

    if(evse_mode2.relay_state == close){
        evse_mode2.evse_relay_ctrl(open);
        evse_mode2.relay_state = open;
    }

    if(evse_mode2.p_cp->pwm_state == ENABLE){
        evse_mode2.p_cp->pwm_ctrl(DISABLE);
        evse_mode2.p_cp->pwm_state = DISABLE;
    }

    if(evse_mode2.evse_state != EVSE_IDLE){
        evse_mode2.evse_state = EVSE_IDLE;
        log_i("EVSE_IDLE.");
    }

    switch (cp_state)
    {
    case CP_12V:
        /* code */
        break;
    case CP_9V:
        return EVSE_READY_9V;
    case CP_6V:
        return EVSE_SIM_6V;
    case CP_ERROR:
        return EVSE_CP_ERROR;
    default:
        break;
    }

    return EVSE_IDLE;
}

evse_state_t evse_9v_handle(cp_state_t cp_state)
{
    if(evse_mode2.evse_state == EVSE_FAULT){    // 从错误中恢复时需要的处理
        log_i("Return frome EVSE_FAULT.");
    }

    if(evse_mode2.relay_state == close){
        evse_mode2.evse_relay_ctrl(open);
        evse_mode2.relay_state = open;
    }

    if(evse_mode2.p_cp->pwm_state == DISABLE){
        evse_mode2.p_cp->pwm_ctrl(ENABLE);
        evse_mode2.p_cp->pwm_state = ENABLE;
    }

    if(evse_mode2.evse_state != EVSE_READY_9V){
        evse_mode2.evse_state = EVSE_READY_9V;
        log_i("EVSE_READY_9V.");
    }

    switch (cp_state)
    {
    case CP_12V:
        return EVSE_IDLE;
    case CP_9V:
        break;
    case CP_6V:
        return EVSE_READY_6V;
    case CP_ERROR:
        return EVSE_CP_ERROR;
    default:
        break;
    }

    return EVSE_READY_9V;
}

evse_state_t evse_6v_handle(cp_state_t cp_state)
{
    if(evse_mode2.evse_state == EVSE_FAULT){    // 从错误中恢复时需要的处理
        log_i("Return frome EVSE_FAULT.");
    }

    if(evse_mode2.relay_state == close){
        evse_mode2.evse_relay_ctrl(open);
        evse_mode2.relay_state = open;
    }

    if(evse_mode2.p_cp->pwm_state == DISABLE){
        evse_mode2.p_cp->pwm_ctrl(ENABLE);
        evse_mode2.p_cp->pwm_state = ENABLE;
    }

    if(evse_mode2.evse_state != EVSE_READY_6V){
        evse_mode2.evse_state = EVSE_READY_6V;
        log_i("EVSE_READY_6V.");
    }

    switch (cp_state)
    {
    case CP_12V:
        return EVSE_CP_LOST;
    case CP_9V:
        return EVSE_READY_9V;
    case CP_6V:
        if(SUCCESS == evse_s1_ck()){
            log_d("s1 ck ok.");
            return EVSE_CHARGING;
        }else{
            log_d("s1 ck error.");
            break;
        }
    case CP_ERROR:
        return EVSE_CP_ERROR;
    default:
        break;
    }

    return EVSE_READY_6V;
}

// 从12V直接进入6V的情况
evse_state_t evse_sim_6v_handle(cp_state_t cp_state)
{
    if(evse_mode2.evse_state == EVSE_FAULT){    // 从错误中恢复时需要的处理
        log_i("Return frome EVSE_FAULT.");
    }

    if(evse_mode2.evse_state != EVSE_SIM_6V){
        evse_mode2.evse_state = EVSE_SIM_6V;
        log_i("EVSE_SIM_6V.");
    }

    switch (cp_state)
    {
    case CP_12V:
        return EVSE_IDLE;
    case CP_9V:
        return EVSE_READY_9V;
    case CP_6V:
    default:
        break;
    }

    return EVSE_SIM_6V;
}

evse_state_t evse_charging_handle(cp_state_t cp_state)
{
    if(evse_mode2.evse_state == EVSE_FAULT){    // 从错误中恢复时需要的处理
        evse_mode2.p_cp->pwm_ctrl(ENABLE);
        evse_mode2.p_cp->pwm_state = ENABLE;
        log_i("Return frome EVSE_FAULT.");
        bos_delay_ms(100);
    }

    if(evse_mode2.relay_state == open){
        evse_mode2.evse_relay_ctrl(close);
        evse_mode2.relay_state = close;
    }

    if(evse_mode2.evse_state != EVSE_CHARGING){
        evse_mode2.evse_state = EVSE_CHARGING;
        log_i("EVSE_CHARGING.");
    }

    switch (cp_state)
    {
    case CP_12V:
        return EVSE_CP_LOST;
    case CP_9V:
        return EVSE_DONE;
    case CP_6V:
    default:
        break;
    }

    return EVSE_CHARGING;
}

// 从6V返回9V
evse_state_t evse_done_handle(cp_state_t cp_state)
{
    if(evse_mode2.evse_state == EVSE_FAULT){    // 从错误中恢复时需要的处理
        evse_mode2.p_cp->pwm_ctrl(ENABLE);
        evse_mode2.p_cp->pwm_state = ENABLE;
        log_i("Return frome EVSE_FAULT.");
        bos_delay_ms(100);
    }

    if(evse_mode2.relay_state == close){
        evse_mode2.evse_relay_ctrl(open);
        evse_mode2.relay_state = open;
    }

    if(evse_mode2.evse_state != EVSE_DONE){
        evse_mode2.evse_state = EVSE_DONE;
        log_i("EVSE_DONE.");
    }

    switch (cp_state)
    {
    case CP_12V:
        return EVSE_IDLE;
    case CP_9V:
        break;
    case CP_6V:
        if(SUCCESS == evse_s1_ck()){
            log_d("s1 ck ok.");
            return EVSE_CHARGING;
        }else{
            log_d("s1 ck error.");
            break;
        }
    default:
        break;
    }

    return EVSE_DONE;
}

// 12V直接进入6V的情况
evse_state_t evse_cp_lost_handle(cp_state_t cp_state)
{
    if(evse_mode2.evse_state == EVSE_FAULT){    // 从错误中恢复时需要的处理
        log_i("Return frome EVSE_FAULT.");
    }

    if(evse_mode2.relay_state == close){
        evse_mode2.evse_relay_ctrl(open);
        evse_mode2.relay_state = open;
    }

    if(evse_mode2.evse_state != EVSE_CP_LOST){
        evse_mode2.evse_state = EVSE_CP_LOST;
        log_i("EVSE_CP_LOST.");
    }

    switch (cp_state)
    {
    case CP_12V:
        break;
    case CP_9V:
        return EVSE_DONE;
    case CP_6V:
        if(SUCCESS == evse_s1_ck()){
            log_d("s1 ck ok.");
            return EVSE_CHARGING;
        }else{
            log_d("s1 ck error.");
            break;
        }
    default:
        break;
    }

    return EVSE_CP_LOST;
}

// CP电平在12V、9V、6V三种状态之外
evse_state_t evse_cp_error_handle(cp_state_t cp_state)
{
    if(evse_mode2.evse_state == EVSE_FAULT){    // 从错误中恢复时需要的处理
        log_i("Return frome EVSE_FAULT.");
    }

    static evse_state_t state_save;
    if(evse_mode2.evse_state != EVSE_CP_ERROR){
        state_save = evse_mode2.evse_state;     // 保存进入CP_ERROR之前的状态(方便返回)
        evse_mode2.evse_state = EVSE_CP_ERROR;
        log_e("EVSE_CP_ERROR.");
    }

    if(evse_mode2.p_cp->pwm_state == ENABLE){
        evse_mode2.p_cp->pwm_ctrl(DISABLE);
        evse_mode2.p_cp->pwm_state = DISABLE;
    }

    if(evse_mode2.relay_state == close){
        evse_mode2.evse_relay_ctrl(open);
        evse_mode2.relay_state = open;
    }

    if(cp_state == CP_ERROR){
        return EVSE_CP_ERROR;
    }
    
    return state_save;
}

evse_state_t evse_fault_handle(cp_state_t cp_state){
    (void)cp_state;

    if(evse_mode2.evse_state != EVSE_FAULT){
        evse_mode2.evse_state = EVSE_FAULT;
        log_e("EVSE_FAULT.");
    }

    if(evse_mode2.p_cp->pwm_state == ENABLE){
        evse_mode2.p_cp->pwm_ctrl(DISABLE);
        evse_mode2.p_cp->pwm_state = DISABLE;
    }

    if(evse_mode2.relay_state == close){
        evse_mode2.evse_relay_ctrl(open);
        evse_mode2.relay_state = open;
    }

    // todo: 按不同的故障优先级依次处理

    return EVSE_FAULT;
}
