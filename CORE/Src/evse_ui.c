#include "evse_ui.h"
#include "st7735.h"
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

UG_GUI gui;
#define UI_FONT FONT_8X14

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

/**
 * @brief  更新UI
 * @todo   根据evse_ui_data结构体的数据来定时更新UI
 */
static void task_entry_ui_upgrade(void *parameter)
{
    evse_ui_init();
    // 显示Booting
    uint16_t str_width = (strlen("System init ...")+2) * UI_FONT.char_width;
    uint16_t x = (160 - str_width) / 2;
    uint16_t y = (128 - UI_FONT.char_height) / 2;
    UG_PutString(x, y, "System init ...");
    /* 等待充电桩主任务完成初始化 */
    while (evse_get_state() == EVSE_REBOOT)
    {
        bos_delay_ms(10);
    }
    UG_FillScreen(C_BLACK);

    // 显示UI框架
    UG_PutString(2, 2, "CUR: ");
    UG_PutString(2, 2+UI_FONT.char_height, "STA: ");
    UG_PutString(2, 2+UI_FONT.char_height*2, "VOL: ");
    UG_PutString(2, 2+UI_FONT.char_height*3, "KWH: ");
    UG_PutString(2, 2+UI_FONT.char_height*4, "ERR: ");

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
            if(evse_ui_data.voltage != evse_ui_data_last.voltage){
                _display_update_voltage(evse_ui_data.voltage);
            }
            // if(evse_ui_data.time != evse_ui_data_last.time){
                // _display_update_time(evse_ui_data.time);
            // }
            if(evse_ui_data.power != evse_ui_data_last.power){
                _display_update_power(evse_ui_data.power);
            }
            if(evse_ui_data.kwh != evse_ui_data_last.kwh){
                _display_update_kwh(evse_ui_data.kwh);
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
    }
}
// bos_task_export(ui_test, task_entry_ui_test, BOS_MAX_PRIORITY, NULL);

static void evse_ui_init(void)
{
    ST7735_BLK_OFF();
    lcd_gpio_config();
    lcd_spi_config();
    ST7735_Init();
    UG_Init(&gui, ST7735_DrawPixel, 160, 128);
    // UG_DriverRegister(DRIVER_FILL_FRAME, ST7735_FillRectangle);
    UG_FontSelect(&FONT_8X14);
    UG_FillScreen(C_BLACK);
    ST7735_BLK_ON();
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
    /* 清除旧的显示区域 */
    UG_DrawFrame(2+UI_FONT.char_width*5, 2, 2+UI_FONT.char_width*8, 2+UI_FONT.char_height, C_BLACK);
    /* 将uint16_t的current转换成字符串 */
    char str_current[5];
    sprintf(str_current, "%2dA", current);
    /* 重新显示 */
    UG_PutString(2+UI_FONT.char_width*5, 2, str_current);
}

static void _display_update_voltage(uint16_t voltage)
{
    uint16_t vol_f = voltage/10;
    if(voltage%10 >= 5){
        vol_f++;
    }
    /* 将uint16_t的voltage转换成字符串 */
    char str_voltage[6];
    sprintf(str_voltage, "%3dV", vol_f);
    /* 清除旧的显示区域 */
    UG_FillFrame(2+UI_FONT.char_width*5, 2+UI_FONT.char_height*2, 2+UI_FONT.char_width*9, 2+UI_FONT.char_height*3, C_BLACK);
    /* 重新显示 */
    UG_PutString(2+UI_FONT.char_width*5, 2+UI_FONT.char_height*2, str_voltage);
}

static void _display_update_state_mode2(uint8_t state)
{
    /* 清除状态文字区域 */
    UG_FillFrame(2+UI_FONT.char_width*5, 2+UI_FONT.char_height, 2+UI_FONT.char_width*14, 2+UI_FONT.char_height*2, C_BLACK);
    _display_update_clock(DISABLE);  // 隐藏时钟图标
    switch (state)
    {
    case EVSE_REBOOT:
        UG_PutString(2+UI_FONT.char_width*5, 2+UI_FONT.char_height, "Booting");
        break;
    case EVSE_IDLE:
    case EVSE_WAIT_PLUGIN:
        UG_PutString(2+UI_FONT.char_width*5, 2+UI_FONT.char_height, "Standby");
        break;
    case EVSE_WAIT_DELAY:
        // _display_update_clock(ENABLE);  // 显示时钟图标
        break;
    case EVSE_9V:
    case EVSE_6V:
    case EVSE_9V_PWM:
    case EVSE_6V_PWM:
        UG_PutString(2+UI_FONT.char_width*5, 2+UI_FONT.char_height, "Connect");
        break;
    case EVSE_CHARGING:
        UG_PutString(2+UI_FONT.char_width*5, 2+UI_FONT.char_height, "Charging");
        break;
    case EVSE_DONE:
        UG_PutString(2+UI_FONT.char_width*5, 2+UI_FONT.char_height, "Full");
        break;
    case EVSE_STOP:
        UG_PutString(2+UI_FONT.char_width*5, 2+UI_FONT.char_height, "Finish");
        break;
    case EVSE_FAULT:
        UG_PutString(2+UI_FONT.char_width*5, 2+UI_FONT.char_height, "Fault");
        break;
    case EVSE_SIM_6V:
        UG_PutString(2+UI_FONT.char_width*5, 2+UI_FONT.char_height, "SIM_6V");
        break;
    default:
        UG_PutString(2+UI_FONT.char_width*5, 2+UI_FONT.char_height, "Unknown");
        break;
    }
}

static void _display_update_fault(evse_ui_fault_t fault)
{
    /* 清除故障显示区域 */
    UG_FillFrame(2+UI_FONT.char_width*5, 2+UI_FONT.char_height*4, 2+UI_FONT.char_width*14, 2+UI_FONT.char_height*5, C_BLACK);
    if (0 != fault.e_cur_leak){
        UG_PutString(2+UI_FONT.char_width*5, 2+UI_FONT.char_height*4, "Leakage");
    }else if (0 != fault.e_vol_err){
        UG_PutString(2+UI_FONT.char_width*5, 2+UI_FONT.char_height*4, "Voltage");
    }else if (0 != fault.e_over_cur){
        UG_PutString(2+UI_FONT.char_width*5, 2+UI_FONT.char_height*4, "Current");
    }else if (0 != fault.e_pe_lost){
        UG_PutString(2+UI_FONT.char_width*5, 2+UI_FONT.char_height*4, "Ground");
    }else if (0 != fault.e_relay_adh){
        UG_PutString(2+UI_FONT.char_width*5, 2+UI_FONT.char_height*4, "Adhesion");
    }else if (0 != fault.e_over_heat){
        UG_PutString(2+UI_FONT.char_width*5, 2+UI_FONT.char_height*4, "OverTemp");
    }else if (0 != fault.e_cp_error){
        UG_PutString(2+UI_FONT.char_width*5, 2+UI_FONT.char_height*4, "CPerror");
    }else if (0 != fault.e_s1_lost){
        UG_PutString(2+UI_FONT.char_width*5, 2+UI_FONT.char_height*4, "Diode");
    }else{
        UG_PutString(2+UI_FONT.char_width*5, 2+UI_FONT.char_height*4, "No fault");
    }
}

static void _display_update_kwh(uint16_t kwh)
{
    float kwh_f = (float)(kwh)/(10.0f);
    /* 将uint16_t的kwh转换成字符串 */
    char str_kwh[6];
    sprintf(str_kwh, "%4.1f", kwh_f);
    /* 清除旧的显示区域 */
    UG_FillFrame(2+UI_FONT.char_width*5, 2+UI_FONT.char_height*3, 2+UI_FONT.char_width*10, 2+UI_FONT.char_height*4, C_BLACK);
    /* 重新显示 */
    UG_PutString(2+UI_FONT.char_width*5, 2+UI_FONT.char_height*3, str_kwh);
}

static void _display_update_power(uint16_t power)
{
    log_d("power: %d", power);
    float power_f = (float)(power)/(10.0f);
    /* 将uint16_t的power转换成字符串 */
    char str_power[6];
    sprintf(str_power, "%4.1f", power_f);
    /* 清除旧的显示区域 */
    UG_FillFrame(2+UI_FONT.char_width*5, 2+UI_FONT.char_height*3, 2+UI_FONT.char_width*10, 2+UI_FONT.char_height*4, C_BLACK);
    /* 重新显示 */
    UG_PutString(2+UI_FONT.char_width*5, 2+UI_FONT.char_height*3, str_power);
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
