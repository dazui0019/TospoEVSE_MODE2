#include "gd32f30x.h"
#include "gd32_hal.h"
#include "ws2812b_port.h"

#define LED_SPI             SPI1
#define LED_CTRL_Pin        GPIO_PIN_15
#define LED_CTRL_GPIO_Port  GPIOB

uint8_t ws2812b_spi_init()
{
    spi_parameter_struct spi_init_struct;
    rcu_periph_clock_enable(RCU_AF);
    rcu_periph_clock_enable(RCU_GPIOB);
    /* LED_CTRL(SPI1_MOSI), PB13: SCK */
    gpio_init(LED_CTRL_GPIO_Port, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, LED_CTRL_Pin);
    gpio_init(GPIOB, GPIO_MODE_AF_PP, GPIO_MODE_AF_PP, GPIO_PIN_13);

    /* SPI config */
    rcu_periph_clock_enable(RCU_SPI1);
    spi_i2s_deinit(LED_SPI);
    /* SPI parameter config */
    spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode          = SPI_MASTER;
    spi_init_struct.frame_size           = SPI_FRAMESIZE_8BIT;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_LOW_PH_1EDGE;
    spi_init_struct.nss                  = SPI_NSS_SOFT;
    spi_init_struct.prescale             = SPI_PSC_4;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;
    spi_init(LED_SPI, &spi_init_struct);
    spi_bidirectional_transfer_config(LED_SPI, SPI_BIDIRECTIONAL_TRANSMIT);    // SPI work in transmit-only mode
    spi_enable(LED_SPI);

    return 0;
}

uint8_t ws2812b_spi_deinit()
{
    spi_i2s_deinit(LED_SPI);
    return 0;
}

uint8_t ws2812b_write_cmd(uint8_t *pData, uint16_t Size)
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
        if(RESET != (SPI_STAT(LED_SPI) & SPI_FLAG_TBE)){
            SPI_DATA(LED_SPI) = (uint32_t)*pTxBuffPtr;
            // spi_i2s_data_transmit(LED_SPI, *pTxBuffPtr);
            TxXferCount--;
            pTxBuffPtr++;
        }
    }

    /* 等待传输完成 */
    while (RESET != (SPI_STAT(LED_SPI) & SPI_FLAG_TRANS)){}

error :
    return errorcode;
}
