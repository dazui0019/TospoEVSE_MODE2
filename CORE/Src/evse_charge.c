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
#include "evse_comm.h"
#include "evse_ui.h"
#include "evse_rgb.h"
#include "evse_beep.h"
#include "drv_rtc.h"
#include "evse_rcd.h"
#include "evse_cfg.h"

#define LOG_TAG "evse.evse"
#include "elog.h"


#if defined(S1_CK_ENABLE)
    // 车端二极管检测
    #define S1_CK_PORT     GPIOA
    #define S1_CK_RCU      RCU_GPIOA
    #define S1_CK_PIN      GPIO_PIN_2
#endif /* S1_CK_ENABLE */

/* 全局变量 */
extern __IO uint16_t *g_p_cp_buff;  // CP采样数据DMA缓冲区
extern cp_t g_cp;                   // CP控制句柄
extern __IO uint16_t g_Vrefint;     // 1.2V参考电压的 ADC 原始值, evse_ac
extern __IO uint16_t g_evse_delay;  // 延时上电时间

#if defined(RFID_ENABLE)
extern __IO uint8_t g_rfid_flag;    // rfid 刷卡标志
#endif /* RFID_ENABLE */

/* 错误标志位 */
extern __IO uint8_t g_overheat_flag;    // evse_ntc
extern __IO uint8_t g_over_cur_flag;    // evse_ac
extern __IO uint8_t g_over_vol_flag;    // evse_ac
extern __IO uint8_t g_under_vol_flag;   // evse_ac
extern __IO uint8_t g_pe_error_flag;    // evse_ac
extern __IO uint8_t rcd_error_flag;    // rcd自检失败
__IO uint8_t cp_lost_flag = false;      // cp丢失
__IO uint8_t cp_error_flag = false;     // cp电平故障
__IO uint8_t s1_lost_flag = false;      // s1二极管缺失

__attribute__((section("CFG_SECTION"), used)) static uint32_t max_cur_index = 0;
static uint8_t max_cur_table[] = MAX_CUR_TABLE_VAL;

evse_t evse = {
    .inited = false,
    .evse_relay_ctrl = evse_relay_ctrl,
    .evse_state = EVSE_REBOOT,
    .relay_state = open,
    .p_cp = &g_cp
};

evse_state_t (*state_func[])(cp_state_t) = {
    /* EVSE_REBOOT,             EVSE_IDLE,              EVSE_WAIT_PLUGIN, */
    evse_idle_handle,           evse_idle_handle,       evse_wait_plugin_handle,
    /* EVSE_9V,                 EVSE_9V_PWM,            EVSE_6V */
    evse_9v_handle,             evse_9v_pwm_handle,     evse_6v_handle,
    /* EVSE_SIM_6V,             EVSE_CHARGING,          EVSE_DONE */
    evse_sim_6v_handle,         evse_charging_handle,   evse_done_handle,
    /* EVSE_WAIT_S2_OPEN        EVSE_STOP               EVSE_WAIT_DELAY */
    evse_wait_s2_open_handle,   evse_stop_handle,       evse_wait_delay
};

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

    log_i("EVSE_REBOOT.");
    
    /* 初始化继电器 */
    evse_relay_init();
    evse_relay_ctrl(open);

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
    g_cp.pwm_ctrl(DISABLE);
    g_cp.ck_ctrl(ENABLE);
    evse_set_max_current(*((uint32_t*)CFG_CUR_START_ADDR)); // 设置最大电流
    log_d("CP init done.");

    #if defined(S1_CK_ENABLE)
    /* 车端二极管检测 */
    s1_ck_init();
    #endif

    evse_rcd_init();
    /* RCD 测试 */
    if(evse_rcd_test() == SET){
        /* RCD 测试成功需要的操作 */
        log_i("RCD test success! ");
    }else{
        /* RCD 测试失败需要的操作 */
        log_e("RCD test fail! ");
        rcd_error_flag = true;
    }

    for(;;){
        // 先检测一下错误标志
        if(g_p_cp_buff != NULL){
            cp_val = evse.p_cp->get_cp_vol(g_p_cp_buff, 10);
            evse.p_cp->state = evse.p_cp->get_cp_state((1.2f*(float)cp_val)/(float)g_Vrefint);
            // log_d("cp_val: %d, cp_vol: %0.2f", evse.p_cp->get_cp_vol(g_p_cp_buff, 10), (1.2f*(float)evse.p_cp->get_cp_vol(g_p_cp_buff, 10))/(float)g_Vrefint);
            g_p_cp_buff = NULL;
            if(SUCCESS == evse_error_ck()){
                evse_state = state_func[evse_state](evse.p_cp->state);
            }else{
                evse_fault_handle(evse.p_cp->state);  // 检测到错误时, 直接调用错误处理函数(不会修改变量evse_state)
            }
        }
        bos_delay_ms(1);
    }
}
bos_task_export(evse_main, task_entry_evse_main, BOS_MAX_PRIORITY, NULL);

/**
 * @brief   错误检测
 * @todo    需要根据不同的错误，进行不同的处理(或许可以直接卡死在这个死循环里，直到错误恢复)
 */
static ErrStatus evse_error_ck(void)
{
    uint8_t error_flag = false;

    for(;;){
        if( g_over_cur_flag|g_pe_error_flag|cp_error_flag|s1_lost_flag|rcd_error_flag|
            cp_lost_flag|g_overheat_flag|g_under_vol_flag|g_over_vol_flag){
            /* 继电器是检测到错误就关闭 */
            if(evse.relay_state == close){
                evse.evse_relay_ctrl(open);
                evse.relay_state = open;
            }
            error_flag = true;
        }

        evse_ui_update(UI_CMD_ERR_CLR_ALL, NULL, NULL);
        /* 不可恢复错误(需要关闭PWM输出) */
        if(g_over_cur_flag == true){    // 过流
            /* PWM需要根据具体的错误类型来控制关闭 */
            if(evse.p_cp->pwm_state == ENABLE){
                evse.p_cp->pwm_ctrl(DISABLE);
                evse.p_cp->pwm_state = DISABLE;
            }
            evse_ui_update(UI_CMD_SET_ERR, FAULT_OVER_CURRENT, NULL);
        }
        
        if(rcd_error_flag == true){
            /* PWM需要根据具体的错误类型来控制关闭 */
            if(evse.p_cp->pwm_state == ENABLE){
                evse.p_cp->pwm_ctrl(DISABLE);
                evse.p_cp->pwm_state = DISABLE;
            }
            evse_ui_update(UI_CMD_SET_ERR, FAULT_LEAKAGE, NULL);
        }
        /* 可恢复错误(不用关闭PWM输出) */
        // if(g_overheat_flag == true){
        //     error_flag = true;
        // }
        
        if(g_under_vol_flag == true){
            evse_ui_update(UI_CMD_SET_ERR, FAULT_UNDER_VOLTAGE, NULL);
        }
        
        if(g_over_vol_flag == true){
            evse_ui_update(UI_CMD_SET_ERR, FAULT_OVER_VOLTAGE, NULL);
        }

        if(g_pe_error_flag == true){
            evse_ui_update(UI_CMD_SET_ERR, FAULT_PE_LOST, NULL);
        }

        if(cp_lost_flag == true){
            evse_ui_update(UI_CMD_SET_ERR, FAULT_CP_LOST, NULL);
        }

        if(cp_error_flag == true){
            evse_ui_update(UI_CMD_SET_ERR, FAULT_CP_ERROR, NULL);
        }

        if(s1_lost_flag == true){
            evse_ui_update(UI_CMD_SET_ERR, FAULT_S1_LOST, NULL);
        }

        if(error_flag == false){
            // evse_beep_ctrl(DISABLE);
            break;
        }else{
            // evse_beep_ctrl(ENABLE);
            return ERROR;
        }
    }
    return SUCCESS;
}

static void s1_ck_init(void)
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

void evse_set_max_current(uint8_t index)
{
    max_cur_index = index;
    evse.p_cp->set_cur(max_cur_table[max_cur_index]);
    evse_ui_update(UI_CMD_UPDATE_CURRENT, max_cur_table[max_cur_index], NULL);
    log_d("Set max current: %d", max_cur_table[max_cur_index]);
}

void evse_max_current_switch(void)
{
    static uint8_t max_cur_index_size = sizeof(max_cur_table)/sizeof(uint8_t);

    if(++max_cur_index == max_cur_index_size){
        max_cur_index = 0;
    }

    evse_set_max_current(max_cur_index);
}

uint8_t evse_get_max_current(void)
{
    return evse.p_cp->current;
}

void evse_set_state(evse_state_t state)
{
    evse.evse_state = state;
    evse_ui_update(UI_CMD_UPDATE_STATE, evse.evse_state, NULL);
    evse_rgb_state_update(evse.evse_state);
}

evse_state_t evse_get_state(void)
{
    return evse.evse_state;
}

evse_state_t evse_idle_handle(cp_state_t cp_state)
{
    if(evse.evse_state == EVSE_FAULT){    // 从错误中恢复时需要的处理
        log_i("Return frome EVSE_FAULT.");
    }

    if(evse.relay_state == close){
        evse.evse_relay_ctrl(open);
        evse.relay_state = open;
    }

    if(evse.p_cp->pwm_state == ENABLE){
        evse.p_cp->pwm_ctrl(DISABLE);
        evse.p_cp->pwm_state = DISABLE;
    }

    if(evse.evse_state != EVSE_IDLE){
        // evse.evse_state = EVSE_IDLE;
        // evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_IDLE, NULL);
        // evse_ui_update(UI_CMD_UPDATE_STATE, EVSE_IDLE, NULL);
        evse_set_state(EVSE_IDLE);
        log_i("EVSE_IDLE.");
    }

    switch (cp_state)
    {
    case CP_12V:
    #if defined(RFID_ENABLE)
        if(g_rfid_flag == true){
            g_rfid_flag = false;
            return EVSE_WAIT_PLUGIN;
        }
    #else
        return EVSE_WAIT_PLUGIN;
    #endif
        break;
    case CP_9V:
        return EVSE_9V;
    case CP_6V:
        return EVSE_SIM_6V;
    case CP_ERROR:
        cp_error_flag = true;
        break;
    default:
        break;
    }

    return EVSE_IDLE;
}

evse_state_t evse_wait_plugin_handle(cp_state_t cp_state)
{
    if(evse.evse_state == EVSE_FAULT){    // 从错误中恢复时需要的处理
        log_i("Return frome EVSE_FAULT.");
    }

    if(evse.relay_state == close){
        evse.evse_relay_ctrl(open);
        evse.relay_state = open;
    }

    if(evse.p_cp->pwm_state == ENABLE){
        evse.p_cp->pwm_ctrl(DISABLE);
        evse.p_cp->pwm_state = DISABLE;
    }

    if(evse.evse_state != EVSE_WAIT_PLUGIN){
        // evse.evse_state = EVSE_WAIT_PLUGIN;
        // evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_WAIT_PLUGIN, NULL);
        // evse_ui_update(UI_CMD_UPDATE_STATE, EVSE_WAIT_PLUGIN, NULL);
        evse_set_state(EVSE_WAIT_PLUGIN);
        log_i("EVSE_WAIT_PLUGIN.");
    }

    switch (cp_state)
    {
    case CP_12V:
    #if defined(RFID_ENABLE)
        if(g_rfid_flag == true){
            g_rfid_flag = false;
            return EVSE_IDLE;
        }
    #endif
        break;
    case CP_9V:
        return (g_evse_delay != 0) ? EVSE_WAIT_DELAY : EVSE_9V_PWM;
    case CP_6V:
        return EVSE_SIM_6V;
    case CP_ERROR:
        cp_error_flag = true;
        break;
    default:
        break;
    }

    return EVSE_WAIT_PLUGIN;
}

evse_state_t evse_9v_handle(cp_state_t cp_state)
{
    if(evse.evse_state == EVSE_FAULT){    // 从错误中恢复时需要的处理
        log_i("Return frome EVSE_FAULT.");
    }

    if(evse.relay_state == close){
        evse.evse_relay_ctrl(open);
        evse.relay_state = open;
    }

    if(evse.p_cp->pwm_state == ENABLE){
        evse.p_cp->pwm_ctrl(DISABLE);
        evse.p_cp->pwm_state = DISABLE;
    }

    if(evse.evse_state != EVSE_9V){
        // evse.evse_state = EVSE_9V;
        // evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_9V, NULL);
        // evse_ui_update(UI_CMD_UPDATE_STATE, EVSE_9V, NULL);
        evse_set_state(EVSE_9V);
        log_i("EVSE_9V.");
    }

    switch (cp_state)
    {
    case CP_12V:
        return EVSE_IDLE;
    case CP_9V:
    #if defined(RFID_ENABLE)
        if(g_rfid_flag == true){
            g_rfid_flag = false;
            return (g_evse_delay != 0) ? EVSE_WAIT_DELAY : EVSE_9V_PWM;
        }
    #else
        return (g_evse_delay != 0) ? EVSE_WAIT_DELAY : EVSE_9V_PWM;
    #endif
        break;
    case CP_6V: // 在未输出PWM的情况下，如果汽车进入CP_6V状态，那么说明是简易导引
        return EVSE_SIM_6V;
    case CP_ERROR:
        cp_error_flag = true;
        break;
    default:
        break;
    }

    return EVSE_9V;
}

evse_state_t evse_wait_delay(cp_state_t cp_state)
{
    static uint8_t delay;

    if(evse.evse_state == EVSE_FAULT){    // 从错误中恢复时需要的处理
        log_i("Return frome EVSE_FAULT.");
    }

    if(evse.relay_state == close){
        evse.evse_relay_ctrl(open);
        evse.relay_state = open;
    }

    if(evse.p_cp->pwm_state == ENABLE){
        evse.p_cp->pwm_ctrl(DISABLE);
        evse.p_cp->pwm_state = DISABLE;
    }

    if(evse.evse_state != EVSE_WAIT_DELAY){
        delay = g_evse_delay;
        // 开启RTC倒计时
        rtc_set_delay_alarm(0, 1, 0);
        evse_set_state(EVSE_WAIT_DELAY);
        log_i("EVSE_WAIT_DELAY.");
    }

    // 开启多次一分钟的倒计时，直到delay为0
    if(rtc_flag_get(RTC_FLAG_ALARM) != RESET){
        rtc_flag_clear(RTC_FLAG_ALARM);
        if(--delay == 0){
            evse_ui_update(UI_CMD_UPDATE_DELAY, g_evse_delay, NULL);
            return EVSE_9V_PWM;
        }
        evse_ui_update(UI_CMD_UPDATE_DELAY, delay, NULL);
        // 重新开启一次一分钟的倒计时
        rtc_set_delay_alarm(0, 1, 0);
    }

    switch (cp_state)
    {
    case CP_12V:
        evse_ui_update(UI_CMD_UPDATE_DELAY, g_evse_delay, NULL);
        return EVSE_IDLE;
    case CP_ERROR:
        cp_error_flag = true;
        break;
    case CP_9V:
    default:
        break;
    }

    return EVSE_WAIT_DELAY;
}

evse_state_t evse_9v_pwm_handle(cp_state_t cp_state)
{
    if(evse.evse_state == EVSE_FAULT){    // 从错误中恢复时需要的处理
        log_i("Return frome EVSE_FAULT.");
    }

    if(evse.relay_state == close){
        evse.evse_relay_ctrl(open);
        evse.relay_state = open;
    }

    if(evse.p_cp->pwm_state == DISABLE){
        evse.p_cp->pwm_ctrl(ENABLE);
        evse.p_cp->pwm_state = ENABLE;
    }

    if(evse.evse_state != EVSE_9V_PWM){
        // evse.evse_state = EVSE_9V_PWM;
        // evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_9V_PWM, NULL);
        // evse_ui_update(UI_CMD_UPDATE_STATE, EVSE_9V_PWM, NULL);
        evse_set_state(EVSE_9V_PWM);
        log_i("EVSE_9V_PWM.");
    }

    switch (cp_state)
    {
    case CP_12V:
        return EVSE_WAIT_PLUGIN;
    case CP_9V:
    #if defined(RFID_ENABLE)
        if(g_rfid_flag == true){
            g_rfid_flag = false;
            return EVSE_9V;
        }
    #endif
        break;
    case CP_6V:
    #if defined(S1_CK_ENABLE)
        if(SUCCESS == evse_s1_ck()){
            log_d("s1_ck ok.");
        }else{
            s1_lost_flag = true;
            log_d("s1_ck error.");
            break;
        }

        if(SET == evse_rcd_test()){
                log_d("rcd ok.");
            }else{
                rcd_error_flag = true;
                log_d("rcd error.");
                break;
            }
        return EVSE_CHARGING;
    #else
        log_d("skip s1_ck.");
        return EVSE_CHARGING;
    #endif
    case CP_ERROR:
        cp_error_flag = true;
        break;
    default:
        break;
    }

    return EVSE_9V_PWM;
}

// 这个状态按道理是不会出现的。
evse_state_t evse_6v_handle(cp_state_t cp_state)
{
    if(evse.evse_state == EVSE_FAULT){    // 从错误中恢复时需要的处理
        log_i("Return frome EVSE_FAULT.");
    }

    if(evse.relay_state == close){
        evse.evse_relay_ctrl(open);
        evse.relay_state = open;
    }

    if(evse.p_cp->pwm_state == ENABLE){
        evse.p_cp->pwm_ctrl(DISABLE);
        evse.p_cp->pwm_state = DISABLE;
    }

    if(evse.evse_state != EVSE_6V){
        // evse.evse_state = EVSE_6V;
        // evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_6V, NULL);
        // evse_ui_update(UI_CMD_UPDATE_STATE, EVSE_6V, NULL);
        evse_set_state(EVSE_6V);
        log_i("EVSE_6V.");
    }

    switch (cp_state)
    {
    case CP_9V:
        return EVSE_9V;
    case CP_6V: // 实际上应该不会出现CP_6V但是还没有PWM输出的情况(相当于CP_12V直接进入CP_6V，也就是简易导引)。
    #if defined(RFID_ENABLE)
        if(g_rfid_flag == true){
            if(evse.p_cp->pwm_state == DISABLE){
                evse.p_cp->pwm_ctrl(ENABLE);
                evse.p_cp->pwm_state = ENABLE;
            }
            g_rfid_flag = false;
        #if defined(S1_CK_ENABLE)
            if(SUCCESS == evse_s1_ck()){
                log_d("s1_ck ok.");
                return EVSE_CHARGING;
            }else{
                log_d("s1_ck error.");
                break;
            }
        #else
            log_d("skip s1_ck.");
            return EVSE_CHARGING;
        #endif
        }
    #else
        if(evse.p_cp->pwm_state == DISABLE){
            evse.p_cp->pwm_ctrl(ENABLE);
            evse.p_cp->pwm_state = ENABLE;
        }
        #if defined(S1_CK_ENABLE)
            if(SUCCESS == evse_s1_ck()){
                log_d("s1_ck ok.");
            }else{
                s1_lost_flag = true;
                log_d("s1_ck error.");
                break;
            }

            if(SET == evse_rcd_test()){
                log_d("rcd ok.");
            }else{
                rcd_error_flag = true;
                log_d("rcd error.");
                break;
            }
            return EVSE_CHARGING;
        #else
            log_d("skip s1_ck.");
            return EVSE_CHARGING;
        #endif
    #endif
        break;
    case CP_12V:
        cp_lost_flag = true;
        break;
    case CP_ERROR:
        cp_error_flag = true;
        break;
    default:
        break;
    }

    return EVSE_6V;
}

// 从12V直接进入6V的情况
evse_state_t evse_sim_6v_handle(cp_state_t cp_state)
{
    if(evse.evse_state == EVSE_FAULT){    // 从错误中恢复时需要的处理
        log_i("Return frome EVSE_FAULT.");
    }

    if(evse.evse_state != EVSE_SIM_6V){
        evse.evse_state = EVSE_SIM_6V;
        // evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_SIM_6V, NULL);
        // evse_ui_update(UI_CMD_UPDATE_STATE, EVSE_SIM_6V, NULL);
        evse_set_state(EVSE_SIM_6V);
        log_i("EVSE_SIM_6V.");
    }

    switch (cp_state)
    {
    case CP_12V:
        return EVSE_IDLE;
    case CP_9V:
        return EVSE_9V;
    case CP_ERROR:
        cp_error_flag = true;
    case CP_6V:
    default:
        break;
    }

    return EVSE_SIM_6V;
}

evse_state_t evse_charging_handle(cp_state_t cp_state)
{
    if(evse.evse_state == EVSE_FAULT){    // 从错误中恢复时需要的处理
        evse.p_cp->pwm_ctrl(ENABLE);
        evse.p_cp->pwm_state = ENABLE;
        log_i("Return frome EVSE_FAULT.");
        // bos_delay_ms(100);
    }

    if(evse.relay_state == open){
        evse.evse_relay_ctrl(close);
        evse.relay_state = close;
    }

    if(evse.evse_state != EVSE_CHARGING){
        // evse.evse_state = EVSE_CHARGING;
        // evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_CHARGING, NULL);
        // evse_ui_update(UI_CMD_UPDATE_STATE, EVSE_CHARGING, NULL);
        evse_cfg_erase();
        evse_cfg_write_cur((uint32_t)(max_cur_index));
        evse_set_state(EVSE_CHARGING);
        log_i("EVSE_CHARGING.");
    }

    #if defined(RFID_ENABLE)
    if(g_rfid_flag == true){  // todo: 进入停止充电状态(不是EVSE_DONE)
        g_rfid_flag = false;
        return EVSE_WAIT_S2_OPEN;
    }
    #endif

    switch (cp_state)
    {
    case CP_9V:
        return EVSE_DONE;
    case CP_12V:
        cp_lost_flag = true;
        break;
    case CP_ERROR:
        cp_error_flag = true;
        break;
    case CP_6V:
    default:
        break;
    }

    return EVSE_CHARGING;
}

// 从6V返回9V
evse_state_t evse_done_handle(cp_state_t cp_state)
{
    if(evse.evse_state == EVSE_FAULT){    // 从错误中恢复时需要的处理
        evse.p_cp->pwm_ctrl(ENABLE);
        evse.p_cp->pwm_state = ENABLE;
        log_i("Return frome EVSE_FAULT.");
        bos_delay_ms(100);
    }

    if(evse.relay_state == close){
        evse.evse_relay_ctrl(open);
        evse.relay_state = open;
    }

    if(evse.evse_state != EVSE_DONE){
        // evse.evse_state = EVSE_DONE;
        // evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_DONE, NULL);
        // evse_ui_update(UI_CMD_UPDATE_STATE, EVSE_DONE, NULL);
        evse_set_state(EVSE_DONE);
        log_i("EVSE_DONE.");
    }

    switch (cp_state)
    {
    case CP_12V:
    #if defined(RFID_ENABLE)
        /* 退出充电后取消RFID鉴权(这里主要是针对在EVSE_DONE状态下刷卡,需要进行的处理) */
        if(g_rfid_flag == true){
            g_rfid_flag = false;
        }
    #endif
        return EVSE_IDLE;
    case CP_9V:
        break;
    case CP_6V:
    #if defined(S1_CK_ENABLE)
        if(SUCCESS == evse_s1_ck()){
            log_d("s1_ck ok.");
        }else{
            s1_lost_flag = true;
            log_d("s1_ck error.");
            break;
        }

        if(SET == evse_rcd_test()){
            log_d("rcd ok.");
        }else{
            rcd_error_flag = true;
            log_d("rcd error.");
            break;
        }
        return EVSE_CHARGING;
    #else
        log_d("skip s1_ck.");
        return EVSE_CHARGING;
    #endif
    case CP_ERROR:
        cp_error_flag = true;
        break;
    default:
        break;
    }

    return EVSE_DONE;
}

evse_state_t evse_fault_handle(cp_state_t cp_state){
    if(evse.evse_state != EVSE_FAULT){
        evse_set_state(EVSE_FAULT);
        log_e("EVSE_FAULT.");
        /* 打印错误标志 */
        // todo: 有一个问题，如果已经在错误状态，那么新增加的错误标志，就不会打印了
        if(cp_lost_flag == true){
            log_e("CP_LOST.");
        }
        if(cp_error_flag == true){
            log_e("CP_ERROR.");
        }
        if(g_over_cur_flag == true){
            log_e("OVER_CUR.");
        }
        if(g_over_vol_flag == true){
            log_e("OVER_VOLT.");
        }
        if(g_overheat_flag == true){
            log_e("OVER_TEMP.");
        }
        if(g_under_vol_flag == true){
            log_e("UNDER_VOLT.");
        }
        if(g_pe_error_flag == true){
            log_e("PE_ERROR.");
        }
        if(s1_lost_flag == true){
            log_e("S1_LOST.");
        }
        if(rcd_error_flag == true){
            log_e("RCD_ERROR.");
        }
    }

    if(cp_lost_flag == true && cp_state != CP_12V){
        cp_lost_flag = false;
    }

    if(cp_error_flag == true && cp_state != CP_ERROR){
        cp_error_flag = false;
    }

    if(s1_lost_flag == true){
        switch (cp_state)
        {
        case CP_12V:    // 拔下枪头或者汽车断开S2，清除s1_lost_flag
        case CP_9V:
            s1_lost_flag = false;
            break;
        case CP_6V:
            if(evse.p_cp->pwm_state == ENABLE){
                s1_lost_flag = (SUCCESS == evse_s1_ck()) ? false : true;
            }
            break;
        default:
            break;
        }
    }
    
    return EVSE_FAULT;
}

/**
 * @brief   充电桩主动停止充电(充电中刷卡)
 * @note    断开继电器
 */
evse_state_t evse_stop_handle(cp_state_t cp_state)
{
    if(evse.evse_state == EVSE_FAULT){    // 从错误中恢复时需要的处理
        log_i("Return frome EVSE_FAULT.");
    }

    if(evse.relay_state == close){
        evse.evse_relay_ctrl(open);
        evse.relay_state = open;
    }

    if(evse.evse_state != EVSE_STOP){
        // evse.evse_state = EVSE_STOP;
        // evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_STOP, NULL);
        // evse_ui_update(UI_CMD_UPDATE_STATE, EVSE_STOP, NULL);
        evse_set_state(EVSE_STOP);
        log_i("EVSE_STOP.");
    }

    switch (cp_state)
    {
    case CP_12V:
        return EVSE_IDLE;
    case CP_9V:
        break;
    case CP_6V:
        return EVSE_SIM_6V; // 未开启PWM的情况下，不能直接进入到CP_6V
    case CP_ERROR:
        cp_error_flag = true;
        break;
    default:
        break;
    }

    return EVSE_STOP;
}

/**
 * @brief   充电桩主动停止充电(充电中刷卡)
 * @note    断开继电器
 * @todo:   这里好像需要检测汽车从CP6V返回到CP9V的时间(暂时不做)
 */
evse_state_t evse_wait_s2_open_handle(cp_state_t cp_state)
{
    if(evse.evse_state == EVSE_FAULT){    // 从错误中恢复时需要的处理
        log_i("Return frome EVSE_FAULT.");
    }

    if(evse.p_cp->pwm_state == ENABLE){
        evse.p_cp->pwm_ctrl(DISABLE);
        evse.p_cp->pwm_state = DISABLE;
    }

    if(evse.evse_state != EVSE_WAIT_S2_OPEN){
        // evse.evse_state = EVSE_WAIT_S2_OPEN;
        // evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_WAIT_S2_OPEN, NULL);
        // evse_ui_update(UI_CMD_UPDATE_STATE, EVSE_WAIT_S2_OPEN, NULL);
        evse_set_state(EVSE_WAIT_S2_OPEN);
        log_i("EVSE_WAIT_S2_OPEN.");
    }

    switch (cp_state)
    {
    case CP_9V:
        return EVSE_STOP;
    case CP_12V:
        cp_lost_flag = true;
    case CP_ERROR:
        cp_error_flag = true;
        break;
    case CP_6V:
    default:
        break;
    }

    // return EVSE_WAIT_S2_OPEN;
    return EVSE_STOP;  // 暂时先直接断开继电器
}
