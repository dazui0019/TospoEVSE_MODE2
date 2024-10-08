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

extern __IO gndd_state_t g_gndd_state;
extern __IO adh_state_t g_adh_state;
extern __IO uint16_t (*g_p_adc2_buff)[2];
extern __IO uint16_t *g_p_cp_buff;
extern cp_t g_cp;
extern uint16_t g_Vrefint;  // 芯片内部1.2V参考电压的 ADC 原始值
extern __IO uint8_t g_overheat_flag;

evse_t evse_mode2 = {
    .inited = false,
    .evse_relay_ctrl = evse_relay_ctrl,
    .evse_state = EVSE_REBOOT,
    .relay_state = open,
    .p_cp = &g_cp
};

evse_state_t (*state_func[])(void) = {
    evse_idle_handle, evse_idle_handle, evse_9v_handle, evse_6v_handle,
    evse_sim_6v_handle, evse_charging_handle, evse_done_handle, evse_cp_lost_handle
};

void evse_init()
{
    ;
}

static void task_entry_evse_main(void *parameter)
{
    uint16_t cp_val, gnd_val;                       // CP电平和接地检测的ADC Raw值。
    uint16_t adh_l = 0, adh_n = 0;
    uint8_t error_flag = false;
    
    /* 错误次数累计 */
    uint8_t gndd_error_cnt = 0;
    uint8_t cp_error_cnt = 0;

    evse_state_t last_evse_state, evse_state;
    last_evse_state = evse_state = EVSE_IDLE;
    
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
    g_cp.init(1000);          // CP输出和检测初始化
    g_cp.set_cur(16);         // 设置最大电流
    g_cp.pwm_ctrl(DISABLE);
    g_cp.ck_ctrl(ENABLE);

    log_d("CP init done.");

    log_i("EVSE_FM_REBOOT.");

    for(;;){
        // 先检测一下错误标志
        error_flag = false;

        if(g_overheat_flag == true){
            error_flag = true;
        }

        if(error_flag == false){
            evse_state = state_func[evse_state]();
        }else{
            evse_state = state_func[evse_state]();
        }
        bos_delay_ms(1);
    }
}
bos_task_export(evse_main, task_entry_evse_main, BOS_MAX_PRIORITY, NULL);

evse_state_t evse_idle_handle(void)
{
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

    /* 获取CP电平和接地检测的ADC Raw值 */
    if(g_p_cp_buff != NULL){
        evse_mode2.p_cp->state = evse_mode2.p_cp->get_cp_state(evse_mode2.p_cp->get_cp_vol(g_p_cp_buff, 10));
        g_p_cp_buff = NULL;
        switch (evse_mode2.p_cp->state)
        {
        case CP_12V:
            /* code */
            break;
        case CP_9V:
            return EVSE_READY_9V;
        case CP_6V:
            return EVSE_READY_6V;
        default:
            return EVSE_CP_ERROR;
        }
    }

    return EVSE_IDLE;
}

evse_state_t evse_9v_handle(void)
{
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

    /* 获取CP电平和接地检测的ADC Raw值 */
    if(g_p_cp_buff != NULL){
        evse_mode2.p_cp->state = evse_mode2.p_cp->get_cp_state(evse_mode2.p_cp->get_cp_vol(g_p_cp_buff, 10));
        g_p_cp_buff = NULL;
        switch (evse_mode2.p_cp->state)
        {
        case CP_12V:
            return EVSE_IDLE;
            break;
        case CP_9V:
            break;
        case CP_6V:
            return EVSE_READY_6V;
        default:
            return EVSE_CP_ERROR;
        }
    }

    return EVSE_READY_9V;
}

evse_state_t evse_6v_handle(void)
{
    if(evse_mode2.p_cp->pwm_state == DISABLE){
        evse_mode2.p_cp->pwm_ctrl(ENABLE);
        evse_mode2.p_cp->pwm_state = ENABLE;
    }

    return EVSE_CHARGING;
}

// 从12V直接进入6V的情况
evse_state_t evse_sim_6v_handle(void)
{
    if(evse_mode2.evse_state != EVSE_SIM_6V){
        evse_mode2.evse_state = EVSE_SIM_6V;
        log_i("EVSE_SIM_6V.");
    }

    /* 获取CP电平和接地检测的ADC Raw值 */
    if(g_p_cp_buff != NULL){
        evse_mode2.p_cp->state = evse_mode2.p_cp->get_cp_state(evse_mode2.p_cp->get_cp_vol(g_p_cp_buff, 10));
        g_p_cp_buff = NULL;
        switch (evse_mode2.p_cp->state)
        {
        case CP_12V:
            return EVSE_IDLE;
            break;
        case CP_9V:
            return EVSE_READY_9V;
        case CP_6V:
            break;
        default:
            return EVSE_CP_ERROR;
        }
    }

    return EVSE_SIM_6V;
}

evse_state_t evse_charging_handle(void)
{
    if(evse_mode2.relay_state == open){
        evse_mode2.evse_relay_ctrl(close);
        evse_mode2.relay_state = close;
    }

    if(evse_mode2.evse_state != EVSE_CHARGING){
        evse_mode2.evse_state = EVSE_CHARGING;
        log_i("EVSE_CHARGING.");
    }

    /* 获取CP电平和接地检测的ADC Raw值 */
    if(g_p_cp_buff != NULL){
        evse_mode2.p_cp->state = evse_mode2.p_cp->get_cp_state(evse_mode2.p_cp->get_cp_vol(g_p_cp_buff, 10));
        g_p_cp_buff = NULL;
        switch (evse_mode2.p_cp->state)
        {
        case CP_12V:
            return EVSE_CP_LOST;
        case CP_9V:
            return EVSE_DONE;
        case CP_6V:
            break;
        default:
            return EVSE_CP_ERROR;
        }
    }

    return EVSE_CHARGING;
}

evse_state_t evse_done_handle(void)
{
    if(evse_mode2.relay_state == close){
        evse_mode2.evse_relay_ctrl(open);
        evse_mode2.relay_state = open;
    }

    if(evse_mode2.evse_state != EVSE_DONE){
        evse_mode2.evse_state = EVSE_DONE;
        log_i("EVSE_DONE.");
    }

    /* 获取CP电平和接地检测的ADC Raw值 */
    if(g_p_cp_buff != NULL){
        evse_mode2.p_cp->state = evse_mode2.p_cp->get_cp_state(evse_mode2.p_cp->get_cp_vol(g_p_cp_buff, 10));
        g_p_cp_buff = NULL;
        switch (evse_mode2.p_cp->state)
        {
        case CP_12V:
            return EVSE_IDLE;
        case CP_9V:
            break;
        case CP_6V:
            return EVSE_CHARGING;
        default:
            return EVSE_CP_ERROR;
        }
    }

    return EVSE_DONE;
}

evse_state_t evse_cp_lost_handle(void)
{
    if(evse_mode2.relay_state == close){
        evse_mode2.evse_relay_ctrl(open);
        evse_mode2.relay_state = open;
    }

    if(evse_mode2.evse_state != EVSE_CP_LOST){
        evse_mode2.evse_state = EVSE_CP_LOST;
        log_i("EVSE_CP_LOST.");
    }

    /* 获取CP电平和接地检测的ADC Raw值 */
    if(g_p_cp_buff != NULL){
        evse_mode2.p_cp->state = evse_mode2.p_cp->get_cp_state(evse_mode2.p_cp->get_cp_vol(g_p_cp_buff, 10));
        g_p_cp_buff = NULL;
        switch (evse_mode2.p_cp->state)
        {
        case CP_12V:
            break;
        case CP_9V:
            return EVSE_DONE;
        case CP_6V:
            return EVSE_CHARGING;
        default:
            return EVSE_CP_ERROR;
        }
    }

    return EVSE_CP_LOST;
}

// evse_state_t error_handle()
// {
//     ;
// }
