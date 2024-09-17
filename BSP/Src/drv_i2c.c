#include "drv_i2c.h"

/*!
    \brief      configure the GPIO ports
    \param[in]  none
    \param[out] none
    \retval     none
*/
void gpio_config(void)
{
    /* enable GPIOB clock */
    rcu_periph_clock_enable(RCU_GPIOB);
    /* enable I2C0 clock */
    rcu_periph_clock_enable(RCU_I2C0);

    /* connect PB6 to I2C0_SCL */
    /* connect PB7 to I2C0_SDA */
    gpio_init(GPIOB, GPIO_MODE_AF_OD, GPIO_OSPEED_50MHZ, GPIO_PIN_6 | GPIO_PIN_7);
}

/*!
    \brief      configure the I2C0 interfaces
    \param[in]  none
    \param[out] none
    \retval     none
*/
void i2c0_master_config(void)
{
    gpio_config();
    /* enable I2C clock */
    rcu_periph_clock_enable(RCU_I2C0);
    /* configure I2C clock */
    i2c_clock_config(I2C0, 400000, I2C_DTCY_2);
    /* configure I2C address */
    i2c_mode_addr_config(I2C0, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, 0x78);  // 这个应该是配置GD32自己的地址
    /* enable I2C0 */
    i2c_enable(I2C0);
    /* enable acknowledge */
    i2c_ack_config(I2C0, I2C_ACK_ENABLE);
}

GD_StatusTypeDef i2c0_send_dma(uint8_t slaver_addr, const uint8_t pData[], uint32_t Size)
{
    (void) slaver_addr;
    if ((pData == NULL) || (Size == 0U))
    {
      return GD_ERROR;
    }

    dma_parameter_struct dma_init_struct;
    /* configure DMA0 */
    rcu_periph_clock_enable(RCU_DMA0);
    i2c_dma_config(I2C0, I2C_DMA_OFF);
    dma_channel_disable(DMA0, DMA_CH5);
    dma_flag_clear(DMA0, DMA_CH5, DMA_INT_FLAG_FTF|DMA_INT_FLAG_HTF);
    /* deinitialize DMA channel5(I2C0 tx) */
    dma_deinit(DMA0, DMA_CH5);
    /* initialize DMA channel5(USART0 tx) */
    dma_init_struct.direction = DMA_MEMORY_TO_PERIPHERAL;
    dma_init_struct.memory_addr = (uint32_t)pData;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_init_struct.number = (uint32_t)Size;
    dma_init_struct.periph_addr = (uint32_t)&I2C_DATA(I2C0);
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_init(DMA0, DMA_CH5, &dma_init_struct);
    /* configure DMA mode */
    dma_circulation_disable(DMA0, DMA_CH5);
    dma_memory_to_memory_disable(DMA0, DMA_CH5);

    /* 开启I2C传输，并发送地址 */
    /* wait until I2C bus is idle */
    while(i2c_flag_get(I2C0, I2C_FLAG_I2CBSY));
    /* send a start condition to I2C bus */
    i2c_start_on_bus(I2C0);
    /* wait until SBSEND bit is set */
    while(!i2c_flag_get(I2C0, I2C_FLAG_SBSEND));
    /* send slave address to I2C bus */
    i2c_master_addressing(I2C0, slaver_addr, I2C_TRANSMITTER);
    /* wait until ADDSEND bit is set */
    while(!i2c_flag_get(I2C0, I2C_FLAG_ADDSEND));
    /* clear the ADDSEND bit */
    i2c_flag_clear(I2C0,I2C_FLAG_ADDSEND);
    /* wait until the transmit data buffer is empty */
    while(SET != i2c_flag_get(I2C0, I2C_FLAG_TBE));

    /* 启动一次DMA传输(这里开始传输数据) */
    dma_channel_enable(DMA0, DMA_CH5);
    i2c_dma_config(I2C0, I2C_DMA_ON);
    
    /* DMA0 channel3 full transfer finish flag */
    while(!dma_flag_get(DMA0, DMA_CH5, DMA_FLAG_FTF));
    dma_flag_clear(DMA0, DMA_CH5, DMA_INT_FLAG_FTF|DMA_INT_FLAG_HTF);

    i2c_stop_on_bus(I2C0);

    return GD_OK;
}