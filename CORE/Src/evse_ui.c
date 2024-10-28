#include "evse_ui.h"
#include "GC9A01.h"
#include "ugui.h"
#include "basic_os.h"
#include "evse_comm.h"
#include <string.h>
#include "evse_charge.h"
#include "printf.h"

#define LOG_TAG "evse.ui"
#include "elog.h"

#define LCD_SPI  SPI2

UG_GUI lcd;

/* 存放UI状态 */
struct __attribute__((packed, aligned(sizeof(uint32_t)))){
    /* EVSE_STATE */
    uint8_t state;
    /* ERROR BIT */
    uint8_t e_cur_leak    :1;
    uint8_t e_over_vol    :1;
    uint8_t e_over_cur    :1;
    uint8_t e_pe_lost     :1;
    uint8_t e_relay_adh   :1;
    uint8_t e_over_heat   :1;
    uint8_t e_cp_error    :1;
    uint8_t e_s1_lost     :1;
    /* DATA */
    uint16_t voltage;
    uint16_t current;
    uint16_t kwh;
} evse_ui_data, evse_ui_data_last;

static void evse_lcd_spi_config(void);
static void evse_lcd_gpio_config(void);
static void evse_ui_init(void);

static void _display_update_current(uint16_t current);
static void _display_update_voltage(uint16_t voltage);
static void _display_update_state(uint8_t state);
static void _display_update_fault(uint8_t fault);
static void _display_update_kwh(uint16_t voltage);
static void _display_update_power(uint16_t voltage);

/**
 * @brief  更新UI
 * @todo   根据evse_ui_data结构体的数据来定时更新UI
 */
static void task_entry_ui_upgrade(void *parameter)
{
    evse_ui_init();    
    /* ----------------默认显示---------------- */
    /* 时钟轮廓 */         
    UG_DrawCircle(120, 120, 40, 0xFFFF);
    UG_DrawLine(35,204,91,148,0xFFFF);
    UG_DrawLine(35,35,91,91,0xFFFF);
    UG_DrawLine(148,91,204,35,0xFFFF);
    UG_DrawLine(149,149,204,204,0xFFFF);
    /* 电压 */
    UG_PutString(16, 115, "220V");
    /* 故障信息 */
    UG_PutString(120 - 48, 240 - 50,"No fault");
    /* 工作状态 */
    UG_PutString(120 - 48, 40, "Standby");
    /* 充电功率/累计电能 */
    UG_PutString(120 - 18,115 - 10,"0.0");
    UG_PutString(120 - 18,115 + 10,"KWh");
    /* 刷卡鉴权状态 */
    UG_PutString(175,115 - 15,"Card");
    GC9A01_drawCross(195,115 + 15, 0xf800, 4);
    for(;;){
        if(0 != memcmp(&evse_ui_data, &evse_ui_data_last, sizeof(evse_ui_data))){
            /* 更新old_ui_data */
            evse_ui_data_last = evse_ui_data;
            /* 刷新UI */
            _display_update_voltage(evse_ui_data_last.voltage);   //更新电压
            // _display_update_current(evse_ui_data_last.current);     //更新电流
            _display_update_state(evse_ui_data_last.state);       //更新工作状态
            // _display_update_fault(evse_ui_data_last.fault);
            _display_update_kwh(evse_ui_data_last.kwh);
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
        bos_delay_ms(1000);
        evse_ui_update(UI_CMD_UPDATE_STATE, EVSE_9V_PWM, NULL);
        vol = 220.0f;
        evse_ui_update(UI_CMD_UPDATE_VOLTAGE, NULL, &vol);
        evse_ui_update(UI_CMD_UPDATE_CURRENT, 16, NULL);
        kwh = 15.1f;
        evse_ui_update(UI_CMD_UPDATE_KWH, NULL, &kwh);

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
bos_task_export(ui_test, task_entry_ui_test, BOS_MAX_PRIORITY, NULL);

static void evse_ui_init(void)
{
    gpio_bit_reset(LCD_BLK_GPIO_Port, LCD_BLK_Pin);
    evse_lcd_gpio_config();
    evse_lcd_spi_config();
    GC9A01_init();
    GC9A01_setRotation(0);
    UG_Init(&lcd, GC9A01_drawPixel, 240, 240);
    UG_FontSelect(&FONT_12X16);
    UG_FillScreen(GC9A01_Color565(0x00, 0x00, 0x00));
    gpio_bit_set(LCD_BLK_GPIO_Port, LCD_BLK_Pin);
}

void evse_lcd_gpio_config(void)
{
    rcu_periph_clock_enable(RCU_AF);
    gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE);
    
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    /* LCD_SCK: PC10, LCD_MOSI: PC12 */
    gpio_pin_remap_config(GPIO_SPI2_REMAP, ENABLE);
    gpio_init(LCD_SCK_GPIO_Port, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, LCD_SCK_Pin);
    gpio_init(LCD_SDA_GPIO_Port, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, LCD_SDA_Pin);
    /* LCD_CS: PA15 */
    gpio_init(LCD_CS_GPIO_Port, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, LCD_CS_Pin);
    gpio_bit_set(LCD_CS_GPIO_Port, LCD_CS_Pin);
    /* LCD_RST:PD2, LCD_DC:PB3, LCD_BLK:PC11 */
    gpio_init(LCD_RST_GPIO_Port, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, LCD_RST_Pin);
    gpio_init(LCD_BLK_GPIO_Port, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, LCD_BLK_Pin);
    gpio_init(LCD_DC_GPIO_Port, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, LCD_DC_Pin);
    gpio_bit_reset(LCD_RST_GPIO_Port, LCD_RST_Pin);
    gpio_bit_reset(LCD_DC_GPIO_Port, LCD_DC_Pin);
}

static void evse_lcd_spi_config(void)
{
    spi_parameter_struct spi_init_struct;
    /* SPI config */
    rcu_periph_clock_enable(RCU_SPI2);
    spi_i2s_deinit(LCD_SPI);
    /* SPI parameter config */
    spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode          = SPI_MASTER;
    spi_init_struct.frame_size           = SPI_FRAMESIZE_8BIT;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_LOW_PH_1EDGE;
    spi_init_struct.nss                  = SPI_NSS_SOFT;
    spi_init_struct.prescale             = SPI_PSC_8;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;
    spi_init(LCD_SPI, &spi_init_struct);
    spi_bidirectional_transfer_config(LCD_SPI, SPI_BIDIRECTIONAL_TRANSMIT);    // SPI work in transmit-only mode
    spi_enable(LCD_SPI);
}

GD_StatusTypeDef evse_lcd_spi_transmit(uint8_t *pData, uint16_t Size)
{
    GD_StatusTypeDef errorcode = GD_OK;
    uint16_t TxXferCount;
    uint8_t* pTxBuffPtr;

    if ((pData == NULL) || (Size == 0U))
    {
        errorcode = GD_ERROR;
        goto error;
    }

    TxXferCount = Size;
    pTxBuffPtr = (uint8_t*)pData;

    while (TxXferCount > 0U){
        if(RESET != (SPI_STAT(LCD_SPI) & SPI_FLAG_TBE)){
            SPI_DATA(LCD_SPI) = (uint32_t)*pTxBuffPtr;
            // spi_i2s_data_transmit(LCD_SPI, *pTxBuffPtr);
            TxXferCount--;
            pTxBuffPtr++;
        }
    }

    /* 等待传输完成 */
    while (RESET != (SPI_STAT(LCD_SPI) & SPI_FLAG_TRANS)){}

error :
    return errorcode;
}

static void _display_update_current(uint16_t current)
{
    GC9A01_fillRect(16, 115, 60, 16, 0x0000);
    /* 将uint16_t的current转换成字符串 */
    char str_current[4];
    sprintf(str_current, "%2d", current);
    /* 字符串拼接将current值字符串和"V"拼在一起 */
    strcat(str_current, "A");
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
    char str_voltage[5];
    sprintf(str_voltage, "%03d", vol_f);
    /* 字符串拼接将voltage值字符串和"V"拼在一起 */
    strcat(str_voltage, "V");
    /* 重新显示 */
    GC9A01_fillRect(16, 115, 60, 16, 0x0000);
    UG_PutString(16, 115, str_voltage);
}

static void _display_update_state(uint8_t state)
{
    GC9A01_fillRect(120 - 48, 40, 96, 16, 0x0000);  // 清除状态文字
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
    }
}

static void _display_update_fault(uint8_t fault)
{
    GC9A01_fillRect(120 - 48, 240 - 50, 100, 16, 0x0000);
    if ((fault & (0x01 << 0)))
    {
        UG_PutString(120 - 48,240 - 50,"Leakage");
    }
    else if ((fault & (0x01 << 1)))
    {
        UG_PutString(120 - 48,240 - 50,"Voltage");
    }
    else if ((fault & (0x01 << 2)))
    {
        UG_PutString(120 - 48,240 - 50,"Current");
    }
    else if ((fault & (0x01 << 3)))
    {
        UG_PutString(120 - 48,240 - 50,"Ground");
    }
    else if ((fault & (0x01 << 4)))
    {
        UG_PutString(120 - 48,240 - 50,"Adhesion");
    }
    else if ((fault & (0x01 << 5)))
    {
        UG_PutString(120 - 48,240 - 50,"OverTemp");
    }
    else if ((fault & (0x01 << 6)))
    {
        UG_PutString(120 - 48,240 - 50,"CPerror");
    }
    else if ((fault & (0x01 << 7)))
    {
        UG_PutString(120 - 48,240 - 50,"Diode");
    }
    else
    {
        UG_PutString(120 - 48,240 - 50,"No fault");
    }

}

static void _display_update_kwh(uint16_t kwh)
{
    float kwh_f = (float)(kwh)/(10.0f);
    /* 将uint16_t的voltage转换成字符串 */
    char str_kwh[6];
    sprintf(str_kwh, "%04.1f", kwh_f);
    GC9A01_fillRect(120 - 26, 115 - 10, 52, 16, 0x0000);
    UG_PutString(120 - 26, 115 - 10, str_kwh);
}

static void _display_update_power(uint16_t power)
{
    
}

/**
 * @brief  更新UI状态结构体
 * @param[in] {cmd} 功能码
 * @param[in] {arg_int} 对于不同的功能码有不同的作用
 * @param[in] {arg_ptr} 用来传递float变量指针，如电压、电流
 */
uint8_t evse_ui_update(uint8_t cmd, uint8_t arg_int, void *arg_ptr)
{
    union{
        uint16_t two_byte;
        uint8_t byte[2];
    }temp_data;
    
    switch (cmd)
    {
    case UI_CMD_UPDATE_STATE:
        evse_ui_data.state = arg_int;
    case UI_CMD_UPDATE_VOLTAGE:
        evse_ui_data.voltage = (uint16_t)((*(float*)arg_ptr)*10.0f);
        break;
    case UI_CMD_UPDATE_CURRENT:
        evse_ui_data.current = arg_int;
        break;
    case UI_CMD_UPDATE_KWH:
        evse_ui_data.kwh = (uint16_t)((*(float*)arg_ptr)*10.0f);
        log_d("kwh: %d.", evse_ui_data.kwh);
        break;
    default:
        log_e("unknown cmd: 0x%02X.", cmd);
        break;
    }
    
    return 0;
}
