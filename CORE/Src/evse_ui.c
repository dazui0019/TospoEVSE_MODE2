#include "evse_ui.h"
#include "GC9A01.h"
#include "ugui.h"
#include "basic_os.h"
#include "evse_comm.h"
#include <string.h>
#include "evse_charge.h"
#include "printf.h"
#include "lcd_port.h"
#include "EventRecorder.h"
#include "evse_charge.h"

#define LOG_TAG "evse.ui"
#include "elog.h"

UG_GUI lcd;

evse_ui_data_t evse_ui_data = {
    .state          = EVSE_REBOOT,
    .time           = 0,
    .voltage        = 0,
    .current        = 0,
    .power          = 0,
    .kwh            = 0,
    .fault          = 0,
};
evse_ui_data_t evse_ui_data_last;

static void evse_ui_init(void);

static void _display_update_current(uint16_t current);
static void _display_update_voltage(uint16_t voltage);
static void _display_update_state_mode2(uint8_t state);
static void _display_update_state_mode3(uint8_t state);
static void _display_update_fault(evse_ui_fault_t fault);
static void _display_update_kwh(uint16_t voltage);
static void _display_update_power(uint16_t voltage);
static void _display_update_clock(ControlStatus status);
static void _display_update_time(uint16_t time);
static void _display_update_all(evse_ui_data_t* ui_data);

/**
 * @brief  更新UI
 * @todo   根据evse_ui_data结构体的数据来定时更新UI
 */
static void task_entry_ui_upgrade(void *parameter)
{
    evse_ui_init();
    // 显示Booting
    UG_FontSelect(&FONT_12X20);
    UG_PutString(20, 110, "System init ...");
    /* 等待充电桩主任务完成初始化 */
    while (evse_get_state() == EVSE_REBOOT)
    {
        bos_delay_ms(10);
    }
    UG_FillScreen(GC9A01_Color565(0x00, 0x00, 0x00));
    /* ----------------默认显示---------------- */
    UG_FontSelect(&FONT_12X16);
    /* 时钟轮廓 */
    UG_DrawCircle(120, 120, 40, 0xFFFF);
    UG_DrawLine(35,204,91,148,0xFFFF);
    UG_DrawLine(35,35,91,91,0xFFFF);
    UG_DrawLine(148,91,204,35,0xFFFF);
    UG_DrawLine(149,149,204,204,0xFFFF);
    UG_PutString(120 - 12, 115 + 10, "kW");

    _display_update_all(&evse_ui_data);
    evse_ui_data_last = evse_ui_data;
    
    for(;;){
        if(0 != memcmp(&evse_ui_data, &evse_ui_data_last, sizeof(evse_ui_data_t))){
            if(evse_ui_data.state != evse_ui_data_last.state){
                _display_update_state_mode2(evse_ui_data.state);
            }
            if(evse_ui_data.current != evse_ui_data_last.current){
                _display_update_current(evse_ui_data.current);
            }
            // if(evse_ui_data.voltage != evse_ui_data_last.voltage){
            //     _display_update_voltage(evse_ui_data.voltage);
            // }
            if(evse_ui_data.time != evse_ui_data_last.time){
                _display_update_time(evse_ui_data.time);
            }
            if(evse_ui_data.power != evse_ui_data_last.power){
                _display_update_power(evse_ui_data.power);
            }
            if(0 != memcmp(&evse_ui_data.fault, &evse_ui_data_last.fault, sizeof(evse_ui_fault_t))){
                _display_update_fault(evse_ui_data.fault);
            }
            /* 更新 evse_ui_data_last */
            evse_ui_data_last = evse_ui_data;
        }
        bos_delay_ms(10);
    }
}
bos_task_export(ui_upgrade, task_entry_ui_upgrade, BOS_MAX_PRIORITY, NULL);

static void task_entry_ui_test(void *parameter)
{
    float kwh = 15.2f;
    float vol = 220.0f;
    for(;;){
        bos_delay_ms(1000);
        evse_ui_update(UI_CMD_UPDATE_STATE, EVSE_IDLE, NULL);
        vol = 220.6f;
        evse_ui_update(UI_CMD_UPDATE_VOLTAGE, NULL, &vol);
        evse_ui_update(UI_CMD_UPDATE_CURRENT, 6, NULL);
        kwh = 6.2f;
        evse_ui_update(UI_CMD_UPDATE_KWH, NULL, &kwh);
        evse_ui_update(UI_CMD_UPDATE_TIME, 3000, NULL);
        bos_delay_ms(1000);
        evse_ui_update(UI_CMD_UPDATE_STATE, EVSE_9V_PWM, NULL);
        vol = 220.0f;
        evse_ui_update(UI_CMD_UPDATE_VOLTAGE, NULL, &vol);
        evse_ui_update(UI_CMD_UPDATE_CURRENT, 16, NULL);
        kwh = 15.1f;
        evse_ui_update(UI_CMD_UPDATE_KWH, NULL, &kwh);
        evse_ui_update(UI_CMD_UPDATE_TIME, 0, NULL);

        // _display_update_current(16);
        // _display_update_state(EVSE_9V_PWM);
        // _display_update_fault(0);
        // _display_update_kwh(0);
        // bos_delay_ms(1000);
        // _display_update_current(6);
        // _display_update_state(EVSE_IDLE);
        // _display_update_fault(2);
        // _display_update_kwh(151);
        // _display_update_voltage(220);
        // bos_delay_ms(1000);
    }
}
// bos_task_export(ui_test, task_entry_ui_test, BOS_MAX_PRIORITY, NULL);

static void evse_ui_init(void)
{
    gpio_bit_reset(LCD_BLK_GPIO_Port, LCD_BLK_Pin);
    lcd_gpio_config();
    lcd_spi_config();
    GC9A01_init();
    GC9A01_setRotation(0);
    UG_Init(&lcd, GC9A01_drawPixel, 240, 240);
    UG_FillScreen(GC9A01_Color565(0x00, 0x00, 0x00));
    gpio_bit_set(LCD_BLK_GPIO_Port, LCD_BLK_Pin);
}

static void _display_update_time(uint16_t time)
{
    char str_time[6];
    sprintf(str_time, "%02d:%02d", time>>8, time&0xFF);
    UG_PutString(195-26, 115, str_time);
}

static void _display_update_clock(ControlStatus status)
{
    switch (status)
    {
    case ENABLE:    // 显示时钟图标
        UG_FillCircle(195, 94, 16, 0x039f);
        UG_FillCircle(195, 94, 15, 0x0000);

        UG_DrawLine(195, 94, 195, 94-7, 0xFFFF);
        UG_DrawLine(195, 94, 195+10, 94, 0xFFFF);
        break;
    case DISABLE:   // 隐藏时钟图标
        UG_FillCircle(195, 94, 16, 0x0000);
    default:
        break;
    }
}

static void _display_update_current(uint16_t current)
{
    GC9A01_fillRect(16, 115, 60, 16, 0x0000);
    /* 将uint16_t的current转换成字符串 */
    char str_current[5];
    sprintf(str_current, "%2dA", current);
    /* 重新显示 */
    UG_PutString(16, 115, str_current);
}

static void _display_update_voltage(uint16_t voltage)
{
    uint16_t vol_f = voltage/10;
    if(voltage%10 > 5){
        vol_f++;
    }
    /* 将uint16_t的voltage转换成字符串 */
    char str_voltage[6];
    sprintf(str_voltage, "%3dV", vol_f);
    /* 重新显示 */
    GC9A01_fillRect(16, 115, 60, 16, 0x0000);
    UG_PutString(16, 115, str_voltage);
}

static void _display_update_state_mode2(uint8_t state)
{
    GC9A01_fillRect(120 - 48, 40, 104, 16, 0x0000);  // 清除状态文字
    _display_update_clock(DISABLE);  // 隐藏时钟图标
    switch (state)
    {
    case EVSE_REBOOT:
        /* code */
        UG_PutString(120 - 48,40,"Booting");
        break;
    case EVSE_IDLE:
    case EVSE_WAIT_PLUGIN:
        /* code */
        UG_PutString(120 - 48,40,"Standby");
        break;
    case EVSE_WAIT_DELAY:
        _display_update_clock(ENABLE);  // 显示时钟图标
    case EVSE_9V:
    case EVSE_6V:
    case EVSE_9V_PWM:
    case EVSE_6V_PWM:
        /* code */
        UG_PutString(120 - 48,40,"Connect");
        break;
    case EVSE_CHARGING:
        /* code */
        UG_PutString(120 - 48,40,"Charging");
        break;
    case EVSE_DONE:
        /* code */
        UG_PutString(120 - 48+24,40,"Full");
        break;
    case EVSE_STOP:
        /* code */
        UG_PutString(120 - 48,40,"Finish");
        break;
    case EVSE_FAULT:
        /* code */
        UG_PutString(120 - 48+12,40,"Fault");
        break;
    case EVSE_SIM_6V:
        /* code */
        UG_PutString(120 - 48+10,40,"SIM_6V");
        break;
    default:
        UG_PutString(120 - 48,40,"Unknown");
        break;
    }
}

static void _display_update_state_mode3(uint8_t state)
{
    GC9A01_fillRect(120 - 48, 40, 104, 16, 0x0000);  // 清除状态文字
    GC9A01_fillRect(195, 115 + 15, 24, 11, 0x0000); // 清除鉴权图标
    switch (state)
    {
    case EVSE_IDLE:
        /* code */
        UG_PutString(120 - 48,40,"Standby");
        //card
        GC9A01_drawCross(195,115 + 15,0xf800, 4);
        break;
    case EVSE_WAIT_PLUGIN:
        /* code */
        UG_PutString(120 - 48,40,"nConnect");
        GC9A01_drawCheckMark(195,115 + 15,0x07e0, 4);
        break;
    case EVSE_9V:
        /* code */
        UG_PutString(120 - 48,40,"Connect");
        GC9A01_drawCross(195,115 + 15,0xf800, 4);
        break;
    case EVSE_9V_PWM:
        /* code */
        UG_PutString(120 - 48,40,"Connect");
        GC9A01_drawCheckMark(195,115 + 15,0x07e0, 4);
        break;
    case EVSE_CHARGING:
        /* code */
        UG_PutString(120 - 48,40,"Charging");
        break;
    case EVSE_DONE:
        /* code */
        UG_PutString(120 - 48,40,"Full");
        break;
    case EVSE_STOP:
        /* code */
        UG_PutString(120 - 48,40,"Finish");
        break;
    case EVSE_FAULT:
        /* code */
        UG_PutString(120 - 48,40,"Fault");
        break;
    default:
        UG_PutString(120 - 48,40,"Unknown");
        break;
    }
}

static void _display_update_fault(evse_ui_fault_t fault)
{
    GC9A01_fillRect(120 - 48, 240 - 50, 100, 16, 0x0000);
    if (0 != fault.e_cur_leak){
        UG_PutString(120 - 48,240 - 50,"Leakage");
    }else if (0 != fault.e_vol_err){
        UG_PutString(120 - 48,240 - 50,"Voltage");
    }else if (0 != fault.e_over_cur){
        UG_PutString(120 - 48,240 - 50,"Current");
    }else if (0 != fault.e_pe_lost){
        UG_PutString(120 - 48,240 - 50,"Ground");
    }else if (0 != fault.e_relay_adh){
        UG_PutString(120 - 48,240 - 50,"Adhesion");
    }else if (0 != fault.e_over_heat){
        UG_PutString(120 - 48,240 - 50,"OverTemp");
    }else if (0 != fault.e_cp_error){
        UG_PutString(120 - 48,240 - 50,"CPerror");
    }else if (0 != fault.e_s1_lost){
        UG_PutString(120 - 48+12,240 - 50,"Diode");
    }else{
        UG_PutString(120 - 48,240 - 50,"No fault");
    }
}

static void _display_update_kwh(uint16_t kwh)
{
    float kwh_f = (float)(kwh)/(10.0f);
    /* 将uint16_t的voltage转换成字符串 */
    char str_kwh[6];
    sprintf(str_kwh, "%4.1f", kwh_f);
    GC9A01_fillRect(120 - 26, 115 - 10, 52, 16, 0x0000);
    UG_PutString(120 - 26, 115 - 10, str_kwh);
}

static void _display_update_power(uint16_t power)
{
    log_d("power: %d", power);
    float power_f = (float)(power)/(10.0f);
    /* 将uint16_t的voltage转换成字符串 */
    char str_kwh[6];
    sprintf(str_kwh, "%4.1f", power_f);
    GC9A01_fillRect(120 - 26, 115 - 10, 52, 16, 0x0000);
    UG_PutString(120 - 26, 115 - 10, str_kwh);
}

static void _display_update_all(evse_ui_data_t* ui_data)
{
    _display_update_state_mode2(ui_data->state);
    _display_update_current(ui_data->current);
    // _display_update_voltage(ui_data->voltage);
    _display_update_power(ui_data->power);
    _display_update_time(ui_data->time);
    _display_update_fault(ui_data->fault);
}
/**
 * @brief  更新UI状态结构体
 * @param[in] {cmd} 功能码
 * @param[in] {arg_int} 对于不同的功能码有不同的作用
 * @param[in] {arg_ptr} 用来传递float变量指针，如电压、电流
 */
uint8_t evse_ui_update(uint8_t cmd, uint16_t arg_int, void *arg_ptr)
{
    union{
        uint16_t two_byte;
        uint8_t byte[2];
    }temp_data;
    
    switch (cmd)
    {
    case UI_CMD_UPDATE_STATE:
        evse_ui_data.state = (uint8_t)arg_int;
        break;
    case UI_CMD_UPDATE_VOLTAGE:
        evse_ui_data.voltage = (uint16_t)((*(float*)arg_ptr)*10.0f);
        break;
    case UI_CMD_UPDATE_CURRENT:
        evse_ui_data.current = arg_int;
        break;
    case UI_CMD_UPDATE_KWH:
        evse_ui_data.kwh = (uint16_t)((*(float*)arg_ptr)*10.0f);
        break;
    case UI_CMD_UPDATE_TIME:
        temp_data.two_byte = arg_int/60; // 小时
        if(temp_data.two_byte>99){
            temp_data.two_byte = 0x633C; // 超过99小时，显示99:60
        }else{
            temp_data.two_byte = (temp_data.two_byte<<8) | arg_int%60; // 分钟
        }
        evse_ui_data.time = temp_data.two_byte;
        break;
    case UI_CMD_UPDATE_POWER:
        evse_ui_data.power = (uint16_t)((*(float*)arg_ptr)/100.0f);
        break;
    case UI_CMD_ERR_CLR_ALL:
        evse_ui_data.fault.e_over_cur = 0;
        evse_ui_data.fault.e_vol_err = 0;
        evse_ui_data.fault.e_over_heat = 0;
        evse_ui_data.fault.e_pe_lost = 0;
        evse_ui_data.fault.e_relay_adh = 0;
        evse_ui_data.fault.e_cp_error = 0;
        evse_ui_data.fault.e_cur_leak = 0;
        evse_ui_data.fault.e_s1_lost = 0;
        break;
    case UI_CMD_SET_ERR:
        switch (arg_int)
        {
        case FAULT_OVER_CURRENT:
            evse_ui_data.fault.e_over_cur = 1;
            break;
        case FAULT_UNDER_VOLTAGE:
        case FAULT_OVER_VOLTAGE:
            evse_ui_data.fault.e_vol_err = 1;
            break;
        case FAULT_OVER_HEAT:
            evse_ui_data.fault.e_over_heat = 1;
            break;
        case FAULT_PE_LOST:
            evse_ui_data.fault.e_pe_lost = 1;
            break;
        case FAULT_RELAY_ADH:
            evse_ui_data.fault.e_relay_adh = 1;
            break;
        case FAULT_CP_LOST:
        case FAULT_CP_ERROR:
            evse_ui_data.fault.e_cp_error = 1;
            break;
        case FAULT_LEAKAGE:
            evse_ui_data.fault.e_cur_leak = 1;
            break;
        case FAULT_S1_LOST:
            evse_ui_data.fault.e_s1_lost = 1;
            break;
        default:
            break;
        }
        break;
    case UI_CMD_RESET_ERR:
        switch (arg_int)
        {
        case FAULT_OVER_CURRENT:
            evse_ui_data.fault.e_over_cur = 0;
            break;
        case FAULT_UNDER_VOLTAGE:
        case FAULT_OVER_VOLTAGE:
            evse_ui_data.fault.e_vol_err = 0;
            break;
        case FAULT_OVER_HEAT:
            evse_ui_data.fault.e_over_heat = 0;
            break;
        case FAULT_PE_LOST:
            evse_ui_data.fault.e_pe_lost = 0;
            break;
        case FAULT_RELAY_ADH:
            evse_ui_data.fault.e_relay_adh = 0;
            break;
        case FAULT_CP_LOST:
        case FAULT_CP_ERROR:
            evse_ui_data.fault.e_cp_error = 0;
            break;
        case FAULT_LEAKAGE:
            evse_ui_data.fault.e_cur_leak = 0;
            break;
        case FAULT_S1_LOST:
            evse_ui_data.fault.e_s1_lost = 0;
            break;
        default:
            break;
        }
        break;
    default:
        log_e("unknown cmd: 0x%02X.", cmd);
        break;
    }
    
    return 0;
}
