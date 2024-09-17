#include "gd32f30x.h"
#include "drv_uart.h"
#include "gd32f30x_usart.h"
#include "printf.h"
#include "delay.h"

/* private variables */
//                                                     USART0               USART1              USART2              UART3               };
const static uint32_t USARTx[COMx]                  = {USART0,              USART1,             USART2,             UART3,              };
const static uint32_t USART_PORT[COMx]              = {USART0_GPIO_PORT,    USART1_GPIO_PORT,   USART2_GPIO_PORT,   UART3_GPIO_PORT,    };
const static uint32_t USART_PIN_RX[COMx]            = {USART0_RX_PIN,       USART1_RX_PIN,      USART2_RX_PIN,      UART3_RX_PIN,       };
const static uint32_t USART_PIN_TX[COMx]            = {USART0_TX_PIN,       USART1_TX_PIN,      USART2_TX_PIN,      UART3_TX_PIN,       };
const static rcu_periph_enum USART_GPIO_CLK[COMx]   = {USART0_GPIO_CLK,     USART1_GPIO_CLK,    USART2_GPIO_CLK,    UART3_GPIO_CLK,     };
const static rcu_periph_enum USART_CLK[COMx]        = {RCU_USART0,          RCU_USART1,         RCU_USART2,         RCU_UART3,          };
const static IRQn_Type USART_IRQ[COMx]              = {USART0_IRQn,         USART1_IRQn,        USART2_IRQn,        UART3_IRQn,         };
const static uint32_t DMAx_Rx[COMx]                 = {DMA0,                DMA0,               DMA0,               DMA1,               };
const static dma_channel_enum RX_DMA_CHx[COMx]      = {DMA_CH4,             DMA_CH5,            DMA_CH2,            DMA_CH2,            };
const static dma_channel_enum TX_DMA_CHx[COMx]      = {DMA_CH3,             DMA_CH6,            DMA_CH1,            DMA_CH4,            };
const static rcu_periph_enum USART_DMA_CLK[COMx]    = {RCU_DMA0,            RCU_DMA0,           RCU_DMA0,           RCU_DMA1,           };
const static IRQn_Type USART_DMA_Rx_IRQ[COMx]       = {DMA0_Channel4_IRQn,  DMA0_Channel5_IRQn, DMA0_Channel2_IRQn, DMA1_Channel2_IRQn, };

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  This function handles UART Communication Timeout. It waits
  *         until a flag is no longer in the specified status.
  * @param  huart  Pointer to a UART_HandleTypeDef structure that contains
  *                the configuration information for the specified UART module.
  * @param  Flag specifies the UART flag to check.
  * @param  Status The actual Flag status (SET or RESET).
  * @param  Tickstart Tick start value
  * @param  Timeout Timeout duration
  * @retval HAL status
  */
static GD_StatusTypeDef UART_WaitOnFlagUntilTimeout(uint32_t usart_periph, usart_flag_enum Flag, FlagStatus Status,
                                                     uint32_t Tickstart, uint32_t Timeout)
{
    /* Wait until flag is set */
    while ((usart_flag_get(usart_periph, Flag) ? SET : RESET) == Status)
    {
        /* Check for the Timeout */
        if (Timeout != GD_MAX_DELAY)
        {
            if (((getTick() - Tickstart) > Timeout) || (Timeout == 0U))
            {
                return GD_TIMEOUT;
            }
        }
    }
    return GD_OK;
}

/**
 * @brief       通用的串口初始化函数
 * @note        开启串口发送和接收功能, 不使能USART中断, UART4不使用该函数
 * @param[in]   com_num 串口号, 具体为usart_typedef_enum里面的值
 * @param[in]   baudval 波特率
 * @retval      none
*/
void gd_usart_init(GD_COMxTypedef com_num, uint32_t baudval)
{
    rcu_periph_clock_enable(RCU_AF);
    /* enable GPIO clock */
    rcu_periph_clock_enable(USART_GPIO_CLK[com_num]);

    /* enable USART clock */
    rcu_periph_clock_enable(USART_CLK[com_num]);

    /* connect port to USARTx_Tx */
    gpio_init(USART_PORT[com_num], GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, USART_PIN_TX[com_num]);

    /* connect port to USARTx_Rx */
    gpio_init(USART_PORT[com_num], GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, USART_PIN_RX[com_num]);

    /* configure USART */
    usart_deinit(USARTx[com_num]);
    usart_baudrate_set(USARTx[com_num], baudval);
    usart_receive_config(USARTx[com_num], USART_RECEIVE_ENABLE);
    usart_transmit_config(USARTx[com_num], USART_TRANSMIT_ENABLE);
    usart_enable(USARTx[com_num]);
}

/**
 * @brief       通用的串口发送初始化函数
 * @note        只开启串口发送功能, 不使能USART中断
 * @param[in]   com_num 串口号, 具体为GD_COMxTypedef里面的值
 * @param[in]   baudval 波特率
 * @retval      none
*/
void gd_usart_tx_init(GD_COMxTypedef com_num, uint32_t baudval)
{
    rcu_periph_clock_enable(RCU_AF);
    /* enable GPIO clock */
    rcu_periph_clock_enable(USART_GPIO_CLK[com_num]);

    /* enable USART clock */
    rcu_periph_clock_enable(USART_CLK[com_num]);

    /* connect port to USARTx_Tx */
    gpio_init(USART_PORT[com_num], GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, USART_PIN_TX[com_num]);

    /* configure USART */
    usart_deinit(USARTx[com_num]);
    usart_baudrate_set(USARTx[com_num], baudval);
    usart_transmit_config(USARTx[com_num], USART_TRANSMIT_ENABLE);
    usart_enable(USARTx[com_num]);
}

/**
 * @brief       通用的串口接收初始化函数
 * @note        只开启串口接收功能, 不使能USART中断
 * @param[in]   com_num 串口号, 具体为GD_COMxTypedef里面的值
 * @param[in]   baudval 波特率
 * @retval      none
*/
void gd_usart_rx_init(GD_COMxTypedef com_num, uint32_t baudval)
{
    rcu_periph_clock_enable(RCU_AF);
    /* enable GPIO clock */
    rcu_periph_clock_enable(USART_GPIO_CLK[com_num]);

    /* enable USART clock */
    rcu_periph_clock_enable(USART_CLK[com_num]);

    /* connect port to USARTx_Rx */
    gpio_init(USART_PORT[com_num], GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, USART_PIN_RX[com_num]);

    /* configure USART */
    usart_deinit(USARTx[com_num]);
    usart_baudrate_set(USARTx[com_num], baudval);
    usart_receive_config(USARTx[com_num], USART_RECEIVE_ENABLE);
    usart_enable(USARTx[com_num]);
}

/**
 * @brief       通用的串口接收DMA初始化函数
 * @note        只开启DMA, 不使能DMA中断
 * @param[in]   com_num 串口号, 具体为GD_COMxTypedef里面的值
 * @param[in]   dma_buffer_addr DMA缓冲区地址
 * @param[in]   dma_buff_length DMA缓冲区深度(大小)
*/
void dma_rx_config(GD_COMxTypedef com_num, uint32_t dma_buffer_addr, uint32_t dma_buff_length)
{
    rcu_periph_clock_enable(USART_DMA_CLK[com_num]); // 这个居然要放在前面。

    dma_parameter_struct dma_init_struct;
    /* deinitialize DMA channel4 (USART0 rx) */
    dma_deinit(DMAx_Rx[com_num], RX_DMA_CHx[com_num]);
    dma_struct_para_init(&dma_init_struct);

    dma_init_struct.direction       = DMA_PERIPHERAL_TO_MEMORY;
    dma_init_struct.memory_addr     = (uint32_t)dma_buffer_addr;
    dma_init_struct.memory_inc      = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.memory_width    = DMA_MEMORY_WIDTH_8BIT;
    dma_init_struct.number          = dma_buff_length;
    dma_init_struct.periph_addr     = ((uint32_t)&USART_DATA(USARTx[com_num]));
    dma_init_struct.periph_inc      = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_width    = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_init_struct.priority        = DMA_PRIORITY_ULTRA_HIGH;
    dma_init(DMAx_Rx[com_num], RX_DMA_CHx[com_num], &dma_init_struct);
    /* enable DMA0 channel3 */
    dma_channel_enable(DMAx_Rx[com_num], RX_DMA_CHx[com_num]);
    /* configure DMA mode */
    dma_circulation_enable(DMAx_Rx[com_num], RX_DMA_CHx[com_num]);
    dma_memory_to_memory_disable(DMAx_Rx[com_num], RX_DMA_CHx[com_num]);
    
    /* enable USART DMA for reception */
    usart_dma_receive_config(USARTx[com_num], USART_RECEIVE_DMA_ENABLE);
}

uint32_t get_uart_num(GD_COMxTypedef com_num){
    return USARTx[com_num];
}

/* Public functions ---------------------------------------------------------*/

/**
 * @brief       使能USART0的发送和接收功能
 * @param[in]   baudval 波特率
 * @retval      none
 * @todo        感觉不是很有必要
*/
void gd_usart0_init(uint32_t baudval){
    gd_usart_init(COM0, baudval);
}

/**
 * @brief       使能USART1的发送和接收功能
 * @param[in]   baudval 波特率
 * @retval      none
 * @todo        感觉不是很有必要
*/
void gd_usart1_init(uint32_t baudval){
    gd_usart_init(COM1, baudval);
}

/**
 * @brief       使能UART4的发送和接收功能
 * @note        PC12 --> UART4_RX
 *              PD2  --> UART4_TX
*/
void gd_uart4_init(uint32_t baudval){
    rcu_periph_clock_enable(RCU_AF);
    /* enable GPIO clock */
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);

    /* enable USART clock */
    rcu_periph_clock_enable(RCU_UART4);

    /* connect port to USARTx_Tx */
    gpio_init(GPIOC, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_12);

    /* connect port to USARTx_Rx */
    gpio_init(GPIOD, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, GPIO_PIN_2);

    /* configure USART */
    usart_deinit(UART4);
    usart_baudrate_set(UART4, baudval);
    usart_receive_config(UART4, USART_RECEIVE_ENABLE);
    usart_transmit_config(UART4, USART_TRANSMIT_ENABLE);
    usart_enable(UART4);
}

/**
 * @brief       使能串口的DMA空闲接收功能
 * @param[in]   com_num 串口号, 具体为usart_typedef_enum里面的值
 * @param[in]   baudval 波特率
 * @param[in]   dma_buff_addr DMA缓冲区地址(转换为uint32_t类型)
 * @param[in]   dma_buff_len DMA缓冲区大小
 * @retval      none
 * @todo        里面的串口初始化函数同时开启了串口接收和发送, 需要改成只接收。
*/
void usart_rx_idle(GD_COMxTypedef com_num, uint32_t baudval, uint32_t dma_buff_addr, uint32_t dma_buff_len)
{
    gd_usart_init(com_num, baudval);
    dma_rx_config(com_num, dma_buff_addr, dma_buff_len);

    usart_flag_clear(USARTx[com_num], USART_FLAG_TC);
    nvic_irq_enable(USART_IRQ[com_num], 0, 0);
    usart_interrupt_enable(USARTx[com_num], USART_INT_IDLE);
    
    nvic_irq_enable(USART_DMA_Rx_IRQ[com_num], 0, 0);
    dma_interrupt_enable(DMAx_Rx[com_num], RX_DMA_CHx[com_num], DMA_CHXCTL_HTFIE|DMA_CHXCTL_FTFIE);
}

/**
 * @brief       使能 USART0 的DMA空闲接收功能
 * @param[in]   baudval 波特率
 * @param[in]   dma_buff_addr DMA缓冲区地址(转换为uint32_t类型)
 * @param[in]   dma_buff_len DMA缓冲区大小
 * @retval      none
 * @todo        里面的串口初始化函数同时开启了串口接收和发送, 需要改成只接收。
*/
void usart0_rx_idle(uint32_t baudval, uint32_t dma_buff_addr, uint32_t dma_buff_len)
{
    gd_usart0_init(baudval);
    dma_rx_config(COM0, dma_buff_addr, dma_buff_len);

    usart_flag_clear(USARTx[COM0], USART_FLAG_TC);
    nvic_irq_enable(USART_IRQ[COM0], 0, 0);
    usart_interrupt_enable(USARTx[COM0], USART_INT_IDLE);
    
    nvic_irq_enable(USART_DMA_Rx_IRQ[COM0], 0, 0);
    dma_interrupt_enable(DMAx_Rx[COM0], RX_DMA_CHx[COM0], DMA_CHXCTL_HTFIE|DMA_CHXCTL_FTFIE);
}

/**
 * @brief       使能 USART1 的DMA空闲接收功能
 * @param[in]   baudval 波特率
 * @param[in]   dma_buff_addr DMA缓冲区地址(转换为uint32_t类型)
 * @param[in]   dma_buff_len DMA缓冲区大小
 * @retval      none
 * @todo        里面的串口初始化函数同时开启了串口接收和发送, 需要改成只接收。
*/
void usart1_rx_idle(uint32_t baudval, uint32_t dma_buff_addr, uint32_t dma_buff_len)
{
    gd_usart1_init(baudval);
    dma_rx_config(COM1, dma_buff_addr, dma_buff_len);

    usart_flag_clear(USARTx[COM1], USART_FLAG_TC);
    nvic_irq_enable(USART_IRQ[COM1], 0, 0);
    usart_interrupt_enable(USARTx[COM1], USART_INT_IDLE);
    
    nvic_irq_enable(USART_DMA_Rx_IRQ[COM1], 0, 0);
    dma_interrupt_enable(DMAx_Rx[COM1], RX_DMA_CHx[COM1], DMA_CHXCTL_HTFIE|DMA_CHXCTL_FTFIE);
}

/**
 * @brief       以轮询的方式从串口接收一个字节的数据
 * @param[in]   usart_periph 串口号, 具体为usart_typedef_enum里面的值
 * @param[out]  pData 接收缓冲区地址
 * @retval      GD_StatusTypeDef
*/
GD_StatusTypeDef usart_receive_byte(uint32_t usart_periph, uint8_t* data)
{
    while (usart_flag_get(usart_periph, USART_FLAG_RBNE) == RESET);
    *data = (uint8_t)usart_data_receive(usart_periph);
    return GD_OK;
}

/**
 * @brief       以轮询的方式从串口接收数据
 * @param[in]   usart_periph 串口号, 具体为usart_typedef_enum里面的值
 * @param[out]  pData 接收缓冲区地址
 * @param[in]   Size 需要接收的数据长度
 * @retval      GD_StatusTypeDef
*/
GD_StatusTypeDef usart_receive(uint32_t usart_periph, uint8_t pData[], uint16_t Size)
{
    uint16_t i = 0;
    while (i < Size){
        if(usart_flag_get(usart_periph, USART_FLAG_RBNE) == SET){
            *(pData+i) = (uint8_t)usart_data_receive(usart_periph);
            i++;
        }
    }
    return GD_OK;
}

/**
  * @brief  Sends an amount of data in blocking mode.
  * @note   When UART parity is not enabled (PCE = 0), and Word Length is configured to 9 bits (M1-M0 = 01),
  *         the sent data is handled as a set of u16. In this case, Size must indicate the number
  *         of u16 provided through pData.
  * @param  huart Pointer to a UART_HandleTypeDef structure that contains
  *               the configuration information for the specified UART module.
  * @param  pData Pointer to data buffer (u8 or u16 data elements).
  * @param  Size  Amount of data elements (u8 or u16) to be sent
  * @param  Timeout Timeout duration
  * @retval GD status
  */
GD_StatusTypeDef GD_UART_Transmit(uint32_t usart_periph, const uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    const uint8_t  *pdata8bits = pData;
    uint16_t count = Size;
    uint32_t tickstart = 0U;
    
    if ((pData == NULL) || (Size == 0U))
    {
        return  GD_ERROR;
    }

    /* Init tickstart for timeout management */
    tickstart = getTick();

    while (count > 0U)
    {
        if (UART_WaitOnFlagUntilTimeout(usart_periph, USART_FLAG_TBE, RESET, tickstart, Timeout) != GD_OK)
        {
            return GD_TIMEOUT;
        }
        USART_DATA(usart_periph) = USART_DATA_DATA & *pdata8bits;
        pdata8bits++;
        count--;
    }

    if (UART_WaitOnFlagUntilTimeout(usart_periph, USART_FLAG_TBE, RESET, tickstart, Timeout) != GD_OK)
    {
        return GD_TIMEOUT;
    }

    return GD_OK;
}

/**
 * @brief  Sends an amount of data in blocking mode.(不带超时检测)
*/
GD_StatusTypeDef UART_Transmit(uint32_t usart_periph, const uint8_t *pData, uint16_t Size)
{
    const uint8_t  *pdata8bits = pData;
    uint16_t count = Size;
    if ((pData == NULL) || (Size == 0U))
    {
        return  GD_ERROR;
    }
    while (count > 0U)
    {
        USART_DATA(usart_periph) = USART_DATA_DATA & *pdata8bits;
        while(RESET == (USART_REG_VAL(usart_periph, USART_FLAG_TBE) & BIT(USART_BIT_POS(USART_FLAG_TBE))));
        pdata8bits++;
        count--;
    }
    return GD_OK;
}

GD_StatusTypeDef GD_UART_PutChar(uint32_t usart_periph, const uint8_t *pData, uint16_t Size)
{
    const uint8_t  *pdata8bits = pData;
    uint16_t count = Size;    
    if ((pData == NULL) || (Size == 0U))
    {
        return  GD_ERROR;
    }

    while (count > 0U)
    {
        USART_DATA(usart_periph) = USART_DATA_DATA & *pdata8bits;
        while(RESET == (USART_REG_VAL(usart_periph, USART_FLAG_TBE) & BIT(USART_BIT_POS(USART_FLAG_TBE))));
        pdata8bits++;
        count--;
    }

    return GD_OK;
}

/**
  * @brief  Receives an amount of data in blocking mode.
  * @note   When UART parity is not enabled (PCE = 0), and Word Length is configured to 9 bits (M1-M0 = 01),
  *         the received data is handled as a set of u16. In this case, Size must indicate the number
  *         of u16 available through pData.
  * @param  huart Pointer to a UART_HandleTypeDef structure that contains
  *               the configuration information for the specified UART module.
  * @param  pData Pointer to data buffer (u8 or u16 data elements).
  * @param  Size  Amount of data elements (u8 or u16) to be received.
  * @param  Timeout Timeout duration
  * @retval GD status
  */
GD_StatusTypeDef GD_UART_Receive(uint32_t usart_periph, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    uint8_t  *pdata8bits = pData;
    uint16_t count = Size;
    uint32_t tickstart = 0U;

  /* Check that a Rx process is not already ongoing */
    if ((pData == NULL) || (Size == 0U))
    {
        return  GD_ERROR;
    }

    /* Init tickstart for timeout management */
    tickstart = getTick();

    pdata8bits  = pData;

    /* Check the remain data to be received */
    while (count > 0U)
    {
        if (UART_WaitOnFlagUntilTimeout(usart_periph, USART_FLAG_RBNE, RESET, tickstart, Timeout) != GD_OK)
        {
            return GD_TIMEOUT;
        }
        *pdata8bits = (uint8_t)usart_data_receive(usart_periph);
        pdata8bits++;
        count--;
    }
    return GD_OK;
}

GD_StatusTypeDef USART0_Transmit_DMA(const uint8_t pData[], uint16_t Size)
{
    if ((pData == NULL) || (Size == 0U))
    {
      return GD_ERROR;
    }
    usart_flag_clear(USART0, USART_FLAG_TC);
    dma_parameter_struct dma_init_struct;
    /* enable DMA0 */
    rcu_periph_clock_enable(RCU_DMA0);
    usart_dma_transmit_config(USART0, USART_TRANSMIT_DMA_DISABLE);
    dma_channel_disable(DMA0, DMA_CH3);
    dma_flag_clear(DMA0, DMA_CH3, DMA_INT_FLAG_FTF|DMA_INT_FLAG_HTF);
    /* deinitialize DMA channel3(USART0 tx) */
    dma_deinit(DMA0, DMA_CH3);
    /* initialize DMA channel3(USART0 tx) */
    dma_init_struct.direction = DMA_MEMORY_TO_PERIPHERAL;
    dma_init_struct.memory_addr = (uint32_t)pData;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_init_struct.number = Size;
    dma_init_struct.periph_addr = (uint32_t)&USART_DATA(USART0);
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_init(DMA0, DMA_CH3, &dma_init_struct);
    /* configure DMA mode */
    dma_circulation_disable(DMA0, DMA_CH3);
    dma_memory_to_memory_disable(DMA0, DMA_CH3);
    
    /* 启动一次传输 */
    dma_channel_enable(DMA0, DMA_CH3);
    usart_dma_transmit_config(USART0, USART_TRANSMIT_DMA_ENABLE);
    
    return GD_OK;
}

/**
 * @brief   USART1 DMA发送
*/
GD_StatusTypeDef USART1_Transmit_DMA(const uint8_t pData[], uint16_t Size)
{
    if ((pData == NULL) || (Size == 0U))
    {
      return GD_ERROR;
    }
    usart_flag_clear(USART1, USART_FLAG_TC);
    dma_parameter_struct dma_init_struct;
    /* enable DMA0 */
    rcu_periph_clock_enable(RCU_DMA0);
    
    dma_channel_disable(DMA0, DMA_CH6);
    dma_flag_clear(DMA0, DMA_CH6, DMA_FLAG_FTF|DMA_FLAG_HTF);
    /* deinitialize DMA channel6(USART1 tx) */
    dma_deinit(DMA0, DMA_CH6);
    dma_init_struct.direction = DMA_MEMORY_TO_PERIPHERAL;
    dma_init_struct.memory_addr = (uint32_t)pData;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_init_struct.number = Size;
    dma_init_struct.periph_addr = (uint32_t)&USART_DATA(USART1);
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_init(DMA0, DMA_CH6, &dma_init_struct);
    /* configure DMA mode */
    dma_circulation_disable(DMA0, DMA_CH6);
    dma_memory_to_memory_disable(DMA0, DMA_CH6);
    
    // nvic_irq_enable(DMA0_Channel6_IRQn, 5, 0);
    // dma_interrupt_enable(DMA0, DMA_CH6, DMA_INT_FLAG_FTF);
    
    /* 启动一次传输 */
    dma_channel_enable(DMA0, DMA_CH6);
    usart_dma_transmit_config(USART1, USART_TRANSMIT_DMA_ENABLE);
    
    return GD_OK;
}
