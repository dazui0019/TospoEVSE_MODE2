#include "lcd_port.h"
#include "basic_os.h"

void lcd_gpio_config(void)
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
    /* LCD_RST:PD2, LCD_DC:PB3, LCD_BLK:PB4 */
    gpio_init(LCD_RST_GPIO_Port, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, LCD_RST_Pin);
    gpio_init(LCD_BLK_GPIO_Port, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, LCD_BLK_Pin);
    gpio_init(LCD_DC_GPIO_Port, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, LCD_DC_Pin);
    gpio_bit_reset(LCD_RST_GPIO_Port, LCD_RST_Pin);
    gpio_bit_reset(LCD_DC_GPIO_Port, LCD_DC_Pin);
}

void lcd_spi_config(void)
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
    spi_init_struct.prescale             = SPI_PSC_2;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;
    spi_init(LCD_SPI, &spi_init_struct);
    spi_bidirectional_transfer_config(LCD_SPI, SPI_BIDIRECTIONAL_TRANSMIT);    // SPI work in transmit-only mode
    spi_enable(LCD_SPI);
}

GD_StatusTypeDef lcd_spi_transmit(uint8_t *pData, uint16_t Size)
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

GD_StatusTypeDef lcd_spi_transmit_dma(uint8_t *pData, uint16_t Size)
{
    GD_StatusTypeDef errorcode = GD_OK;
    dma_parameter_struct dma_init_struct;
    rcu_periph_clock_enable(RCU_DMA1);

    dma_channel_disable(DMA1, DMA_CH1);
    dma_flag_clear(DMA1, DMA_CH1, DMA_FLAG_FTF|DMA_FLAG_HTF);
    /* SPI2 transmit dma config: DMA1,DMA_CH1  */
    dma_deinit(DMA1, DMA_CH1);
    dma_init_struct.periph_addr  = (uint32_t)&SPI_DATA(SPI2);
    dma_init_struct.memory_addr  = (uint32_t)pData;
    dma_init_struct.direction    = DMA_MEMORY_TO_PERIPHERAL;
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_init_struct.priority     = DMA_PRIORITY_LOW;
    dma_init_struct.number       = Size;
    dma_init_struct.periph_inc   = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc   = DMA_MEMORY_INCREASE_ENABLE;
    dma_init(DMA1, DMA_CH1, &dma_init_struct);
    /* configure DMA mode */
    dma_circulation_disable(DMA1, DMA_CH1);
    dma_memory_to_memory_disable(DMA1, DMA_CH1);

    spi_dma_enable(LCD_SPI, SPI_DMA_TRANSMIT);
    dma_channel_enable(DMA1, DMA_CH1);

    while (RESET == (DMA_INTF(DMA1) & DMA_FLAG_ADD(DMA_FLAG_FTF, DMA_CH1)))
    {
        // dma_flag_clear(DMA1, DMA_CH1, DMA_FLAG_FTF);
        bos_delay_ms(1);
    }
    dma_flag_clear(DMA1, DMA_CH1, DMA_FLAG_FTF);

    return errorcode;
}
