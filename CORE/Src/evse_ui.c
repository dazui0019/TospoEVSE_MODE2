#include "evse_ui.h"
#include "GC9A01.h"
#include "ugui.h"
#include "basic_os.h"
#include "evse_comm.h"
#include <string.h>

#define LOG_TAG "evse.ui"
#include "elog.h"

#define LCD_SPI  SPI0

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
    uint16_t power;
    uint16_t kwh;
} evse_ui_data;

static void evse_lcd_spi_config(void);
static void evse_lcd_gpio_config(void);

void evse_ui_init(void)
{
    gpio_bit_reset(LCD_BLK_GPIO_Port, LCD_BLK_Pin);
    evse_lcd_gpio_config();
    evse_lcd_spi_config();
    GC9A01_init();
    GC9A01_setRotation(3);
    UG_Init(&lcd, GC9A01_drawPixel, 240, 240);
    UG_FontSelect(&FONT_12X16);
    UG_FillScreen(GC9A01_Color565(0x00, 0x00, 0x00));
    UG_PutString(60, 120, "Hello, uGUI!");
    gpio_bit_set(LCD_BLK_GPIO_Port, LCD_BLK_Pin);
}

void evse_lcd_gpio_config(void)
{
    rcu_periph_clock_enable(RCU_AF);
    
    rcu_periph_clock_enable(RCU_GPIOA);
    /* SCK: PA5, MOSI: PA7 */
    gpio_init(LCD_SCK_GPIO_Port, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, LCD_SCK_Pin);
    gpio_init(LCD_SDA_GPIO_Port, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, LCD_SDA_Pin);
    /* NSS: PA3 */
    gpio_init(LCD_CS_GPIO_Port, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, LCD_CS_Pin);
    gpio_bit_set(LCD_CS_GPIO_Port, LCD_CS_Pin);
    /* GPIO config: CS/PA2, RST/PA8, DC/PA4, BLK/PA2 */
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
    rcu_periph_clock_enable(RCU_SPI0);
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
        if(RESET != (SPI_STAT(SPI0) & SPI_FLAG_TBE)){
            SPI_DATA(LCD_SPI) = (uint32_t)*pTxBuffPtr;
            // spi_i2s_data_transmit(LCD_SPI, *pTxBuffPtr);
            TxXferCount--;
            pTxBuffPtr++;
        }
    }

    /* 等待传输完成 */
    while (RESET != (SPI_STAT(SPI0) & SPI_FLAG_TRANS)){}

error :
    return errorcode;
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
        // evse_ui_data.voltage = (uint16_t)((*(float*)arg_ptr)/10.0f);
        // log_d("voltage: %d.", (uint16_t)((*(float*)arg_ptr)/10.0f));
        temp_data.two_byte = (uint16_t)((*(float*)arg_ptr)/10.0f);
        evse_ui_data.voltage = (temp_data.byte[0]<<8) | temp_data.byte[1];
        break;
    case UI_CMD_UPDATE_POWER:
        // evse_ui_data.power = (uint16_t)((*(float*)arg_ptr)*10);
        temp_data.two_byte = (uint16_t)((*(float*)arg_ptr));
        evse_ui_data.power = (temp_data.byte[0]<<8) | temp_data.byte[1];
        break;
    case UI_CMD_UPDATE_KWH:
        temp_data.two_byte = (uint16_t)((*(float*)arg_ptr)*10.0f);
        evse_ui_data.kwh = (temp_data.byte[0]<<8) | temp_data.byte[1];
        break;
    default:
        log_e("unknown cmd: 0x%02X.", cmd);
        break;
    }
    
    return 0;
}

/**
 * @brief  更新UI
 * @todo   根据evse_ui_data结构体的数据来定时更新UI
 */
static void task_entry_ui_upgrade(void *parameter)
{
    for(;;){
        bos_delay_ms(1000);
    }
}
// bos_task_export(ui_upgrade, task_entry_ui_upgrade, BOS_MAX_PRIORITY, NULL);
