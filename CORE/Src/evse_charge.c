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

#if defined(RFID_ENABLE)
extern __IO uint8_t g_rfid_flag;
#endif

/* 错误标志位 */
extern __IO uint8_t g_overheat_flag;    // evse_ntc
extern __IO uint8_t g_over_cur_flag;    // evse_ac
extern __IO uint8_t g_over_vol_flag;    // evse_ac
extern __IO uint8_t g_under_vol_flag;   // evse_ac
extern __IO uint8_t g_pe_error_flag;    // evse_ac

evse_t evse = {
    .inited = false,
    .evse_relay_ctrl = evse_relay_ctrl,
    .evse_state = EVSE_REBOOT,
    .relay_state = open,
    .p_cp = &g_cp
};

evse_state_t (*state_func[])(cp_state_t) = {
    /* EVSE_REBOOT,         EVSE_IDLE,              EVSE_WAIT_PLUGIN, */
    evse_idle_handle,       evse_idle_handle,       evse_wait_plugin_handle,
    /* EVSE_9V,             EVSE_9V_PWM,            EVSE_6V */
    evse_9v_handle,         evse_9v_pwm_handle,     evse_6v_handle,
    /* EVSE_SIM_6V,         EVSE_CHARGING,          EVSE_DONE */
    evse_sim_6v_handle,     evse_charging_handle,   evse_done_handle,
    /* EVSE_CP_LOST,        EVSE_CP_ERROR,          EVSE_WAIT_S2_OPEN */
    evse_cp_lost_handle,    evse_cp_error_handle,   evse_wait_s2_open_handle,
    /* EVSE_STOP */
    evse_stop_handle,
};

/**
 * @brief   错误检测
 * @todo    需要根据不同的错误，进行不同的处理
 */
static ErrStatus evse_error_ck(void)
{
    uint8_t error_flag = false;

    for(;;){
        /* 不可恢复错误(需要关闭PWM输出) */
        // if(g_cur_leak_flag == true){
        //     /* PWM需要根据具体的错误类型来控制关闭 */
        //     if(evse.p_cp->pwm_state == ENABLE){
        //         evse.p_cp->pwm_ctrl(DISABLE);
        //         evse.p_cp->pwm_state = DISABLE;
        //     }
        //     error_flag = true;
        // }

        /* 可恢复错误(不用关闭PWM输出) */
        // if(g_overheat_flag == true){
        //     error_flag = true;
        // }
        if(g_under_vol_flag == true){
            error_flag = true;
        }
        if(g_over_vol_flag == true){
            error_flag = true;
        }
        if(g_over_cur_flag == true){
            error_flag = true;
        }
        if(g_pe_error_flag == true){
            error_flag = true;
        }

        if(error_flag == false){
            break;
        }else{
            /* 继电器是检测到错误就关闭 */
            if(evse.relay_state == close){
                evse.evse_relay_ctrl(open);
                evse.relay_state = open;
            }
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
    
    #if defined(S1_CK_ENABLE)
    /* 车端二极管检测 */
    s1_ck_init();
    #endif

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
            evse.p_cp->state = evse.p_cp->get_cp_state(evse.p_cp->get_cp_vol(g_p_cp_buff, 10));
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
            log_d("s1_ck ok.");
            return SUCCESS;
        }
    }
    log_d("s1_ck error.");
    return ERROR;
}

uint8_t evse_get_max_current(void)
{
    return evse.p_cp->current;
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
        evse.evse_state = EVSE_IDLE;
        evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_IDLE, NULL);
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
        return EVSE_CP_ERROR;
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
        evse.evse_state = EVSE_WAIT_PLUGIN;
        evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_WAIT_PLUGIN, NULL);
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
        return EVSE_9V_PWM;
    case CP_6V:
        return EVSE_SIM_6V;
    case CP_ERROR:
        return EVSE_CP_ERROR;
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
        evse.evse_state = EVSE_9V;
        evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_9V, NULL);
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
            return EVSE_9V_PWM;
        }
    #else
        return EVSE_9V_PWM;
    #endif
        break;
    case CP_6V: // 在未输出PWM的情况下，如果汽车进入CP_6V状态，那么说明是简易导引
        return EVSE_SIM_6V;
    case CP_ERROR:
        return EVSE_CP_ERROR;
    default:
        break;
    }

    return EVSE_9V;
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
        evse.evse_state = EVSE_9V_PWM;
        evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_9V_PWM, NULL);
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
            return EVSE_CHARGING;
        }else{
            log_d("s1_ck error.");
            break;
        }
    #else
        log_d("skip s1_ck.");
        return EVSE_CHARGING;
    #endif
    case CP_ERROR:
        return EVSE_CP_ERROR;
    default:
        break;
    }

    return EVSE_9V_PWM;
}

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
        evse.evse_state = EVSE_6V;
        evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_6V, NULL);
        log_i("EVSE_6V.");
    }

    switch (cp_state)
    {
    case CP_12V:
        return EVSE_CP_LOST;
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
                return EVSE_CHARGING;
            }else{
                log_d("s1_ck error.");
                break;
            }
        #else
            log_d("skip s1_ck.");
            return EVSE_CHARGING;
        #endif
    #endif
        break;
    case CP_ERROR:
        return EVSE_CP_ERROR;
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
        evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_SIM_6V, NULL);
        log_i("EVSE_SIM_6V.");
    }

    switch (cp_state)
    {
    case CP_12V:
        return EVSE_IDLE;
    case CP_9V:
        return EVSE_9V;
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
        bos_delay_ms(100);
    }

    if(evse.relay_state == open){
        evse.evse_relay_ctrl(close);
        evse.relay_state = close;
    }

    if(evse.evse_state != EVSE_CHARGING){
        evse.evse_state = EVSE_CHARGING;
        evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_CHARGING, NULL);
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
        evse.evse_state = EVSE_DONE;
        evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_DONE, NULL);
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
            return EVSE_CHARGING;
        }else{
            log_d("s1_ck error.");
            break;
        }
    #else
        log_d("skip s1_ck.");
        return EVSE_CHARGING;
    #endif
    default:
        break;
    }

    return EVSE_DONE;
}

/**
 * @brief 处理CP丢失的情况(CP_12V直接进入CP_6V的情况)
 * @param cp_state CP状态
 * @todo  错误处理还未确定，不知道需不需要做状态恢复。
 * @note  会进入这个状态的只有EVSE_CHARGING和EVSE_WAIT_S2_OPEN, 所以在恢复时, 需要通过这两个状态来具体判断。
 */
evse_state_t evse_cp_lost_handle(cp_state_t cp_state)
{
    __attribute__((used)) static evse_state_t evse_state_last;  // 记录上一次的状态(用于从CP_LOST中恢复)

    if(evse.evse_state == EVSE_FAULT){    // 从错误中恢复时需要的处理
        log_i("Return frome EVSE_FAULT.");
        evse.p_cp->pwm_ctrl(ENABLE);
        evse.p_cp->pwm_state = ENABLE;
    }

    if(evse.relay_state == close){
        evse.evse_relay_ctrl(open);
        evse.relay_state = open;
    }

    if(evse.evse_state != EVSE_CP_LOST){
        evse_state_last = evse.evse_state;
        evse.evse_state = EVSE_CP_LOST;
        evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_CP_LOST, NULL);
        log_i("EVSE_CP_LOST.");
    }

    switch (evse_state_last)
    {
        case EVSE_CHARGING:
            if(cp_state == CP_12V){
                break;
            }else if(cp_state == CP_9V){
                return EVSE_9V_PWM;
            }else if (cp_state == CP_6V){
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
            break;
        case EVSE_WAIT_S2_OPEN:
            if(cp_state == CP_12V){
                break;
            }else if(cp_state == CP_9V){
                return EVSE_STOP;
            }else if (cp_state == CP_6V){
                return EVSE_WAIT_S2_OPEN;
            }
            break;
        case EVSE_IDLE:
        case EVSE_WAIT_PLUGIN:
        case EVSE_9V:
        case EVSE_9V_PWM:
        case EVSE_6V:
        case EVSE_SIM_6V:
        case EVSE_DONE:
        default:
            /* 这些状态是不可能进入CP_LOST的 */
            log_e("evse last state: %d", evse_state_last);
            break;
    }

    return EVSE_CP_LOST;
}

// CP电平在12V、9V、6V三种状态之外
evse_state_t evse_cp_error_handle(cp_state_t cp_state)
{
    if(evse.evse_state == EVSE_FAULT){    // 从错误中恢复时需要的处理
        log_i("Return frome EVSE_FAULT.");
    }

    static evse_state_t state_save;
    if(evse.evse_state != EVSE_CP_ERROR){
        state_save = evse.evse_state;     // 保存进入CP_ERROR之前的状态(方便返回)
        evse.evse_state = EVSE_CP_ERROR;
        evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_CP_ERROR, NULL);
        log_e("EVSE_CP_ERROR.");
    }

    if(evse.p_cp->pwm_state == ENABLE){
        evse.p_cp->pwm_ctrl(DISABLE);
        evse.p_cp->pwm_state = DISABLE;
    }

    if(evse.relay_state == close){
        evse.evse_relay_ctrl(open);
        evse.relay_state = open;
    }

    if(cp_state == CP_ERROR){
        return EVSE_CP_ERROR;
    }
    
    return state_save;
}

evse_state_t evse_fault_handle(cp_state_t cp_state){
    (void)cp_state;

    if(evse.evse_state != EVSE_FAULT){
        evse.evse_state = EVSE_FAULT;
        evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_FAULT, NULL);
        log_e("EVSE_FAULT.");
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
        evse.evse_state = EVSE_STOP;
        evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_STOP, NULL);
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
        evse.evse_state = EVSE_WAIT_S2_OPEN;
        evse_comm_ui_update(UI_CMD_UPDATE_STATE, EVSE_WAIT_S2_OPEN, NULL);
        log_i("EVSE_WAIT_S2_OPEN.");
    }

    switch (cp_state)
    {
    case CP_12V:
        return EVSE_CP_LOST;
    case CP_9V:
        return EVSE_STOP;
    case CP_6V:
    default:
        break;
    }

    // return EVSE_WAIT_S2_OPEN;
    return EVSE_STOP;  // 暂时先直接断开继电器
}
