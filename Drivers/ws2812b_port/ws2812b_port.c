#include "gd32f30x.h"
#include "gd32_hal.h"
#include "ws2812b_port.h"
#include "basic_os.h"

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
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_HIGH_PH_2EDGE;
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

// uint8_t ws2812b_write_cmd(uint8_t *pData, uint16_t Size)
// {
//     GD_StatusTypeDef errorcode = GD_OK;
//     uint16_t TxXferCount;
//     uint8_t* pTxBuffPtr;

//     if ((pData == NULL) || (Size == 0U))
//     {
//         errorcode = GD_ERROR;
//         goto error;
//     }

//     TxXferCount = Size;
//     pTxBuffPtr = (uint8_t*)pData;

//     while (TxXferCount > 0U){
//         if(RESET != (SPI_STAT(LED_SPI) & SPI_FLAG_TBE)){
//             SPI_DATA(LED_SPI) = (uint32_t)*pTxBuffPtr;
//             // spi_i2s_data_transmit(LED_SPI, *pTxBuffPtr);
//             TxXferCount--;
//             pTxBuffPtr++;
//         }
//     }

//     /* 等待传输完成 */
//     while (RESET != (SPI_STAT(LED_SPI) & SPI_FLAG_TRANS)){}

// error :
//     return errorcode;
// }

uint8_t ws2812b_write_cmd(uint8_t *pData, uint16_t Size)
{
    GD_StatusTypeDef errorcode = GD_OK;
    dma_parameter_struct dma_init_struct;
    rcu_periph_clock_enable(RCU_DMA0);

    dma_channel_disable(DMA0, DMA_CH4);
    dma_flag_clear(DMA0, DMA_CH4, DMA_FLAG_FTF|DMA_FLAG_HTF);
    /* SPI2 transmit dma config: DMA0,DMA_CH4  */
    dma_deinit(DMA0, DMA_CH4);
    dma_init_struct.periph_addr  = (uint32_t)&SPI_DATA(SPI1);
    dma_init_struct.memory_addr  = (uint32_t)pData;
    dma_init_struct.direction    = DMA_MEMORY_TO_PERIPHERAL;
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_init_struct.priority     = DMA_PRIORITY_LOW;
    dma_init_struct.number       = Size;
    dma_init_struct.periph_inc   = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc   = DMA_MEMORY_INCREASE_ENABLE;
    dma_init(DMA0, DMA_CH4, &dma_init_struct);
    /* configure DMA mode */
    dma_circulation_disable(DMA0, DMA_CH4);
    dma_memory_to_memory_disable(DMA0, DMA_CH4);

    spi_dma_enable(LED_SPI, SPI_DMA_TRANSMIT);
    dma_channel_enable(DMA0, DMA_CH4);

    while(dma_flag_get(DMA0, DMA_CH4, DMA_FLAG_FTF) == RESET){bos_delay_ms(1);}
    dma_flag_clear(DMA0, DMA_CH4, DMA_FLAG_FTF);

    return errorcode;
}
