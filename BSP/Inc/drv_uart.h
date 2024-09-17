#pragma once

#include "gd32f30x_usart.h"
#include "stdlib.h"
#include "gd32_hal.h"

#define COMx  5U    // 串口数量

/* todo: 把串口号从USART_x改成COMx */
typedef enum 
{
    COM0 = 0,
    COM1 = 1,
    COM2,
    COM3,
    COM4
} GD_COMxTypedef;

/* 定义中断源 */
typedef enum
{
    S_DMA = 0x01,
    S_UART
}UART_ITSource;

/* USART0 */
#define USART0_TX_PIN                   GPIO_PIN_9
#define USART0_RX_PIN                   GPIO_PIN_10
#define USART0_GPIO_PORT                GPIOA
#define USART0_GPIO_CLK                 RCU_GPIOA

/* USART1 */
#define USART1_TX_PIN                   GPIO_PIN_2
#define USART1_RX_PIN                   GPIO_PIN_3
#define USART1_GPIO_PORT                GPIOA
#define USART1_GPIO_CLK                 RCU_GPIOA

/* USART2 */
#define USART2_TX_PIN                   GPIO_PIN_10
#define USART2_RX_PIN                   GPIO_PIN_11
#define USART2_GPIO_PORT                GPIOB
#define USART2_GPIO_CLK                 RCU_GPIOB

/* 48个引脚的芯片下面这些串口不可用 */
/* UART3 */
#define UART3_TX_PIN                   GPIO_PIN_10
#define UART3_RX_PIN                   GPIO_PIN_11
#define UART3_GPIO_PORT                GPIOC
#define UART3_GPIO_CLK                 RCU_GPIOC

/* UART4(有点问题, 两个引脚的端口不一致, 所以不要使用通用的初始化函数, 并且该串口无法使用DMA) */
#define UART4_TX_PIN                   GPIO_PIN_12
#define UART4_RX_PIN                   GPIO_PIN_2
#define UART4_TX_GPIO_PORT             GPIOC
#define UART4_TX_GPIO_CLK              RCU_GPIOC
#define UART4_RX_GPIO_PORT             GPIOD
#define UART4_RX_GPIO_CLK              RCU_GPIOD

#define USART0_RDATA_ADDRESS           ((uint32_t)&USART_DATA(USART0))
#define USART1_RDATA_ADDRESS           ((uint32_t)&USART_DATA(USART1))
#define USART2_RDATA_ADDRESS           ((uint32_t)&USART_DATA(USART2))

/* Clear DATA register */
#define __UART_FLUSH_DATAREGISTER(usartx) USART_DATA(usartx)

/* init funtion */
void gd_usart_tx_init(GD_COMxTypedef com_num, uint32_t baudval);
void gd_usart_rx_init(GD_COMxTypedef com_num, uint32_t baudval);
void gd_usart_init(GD_COMxTypedef com_num, uint32_t baudval);
void gd_usart0_init(uint32_t baudval);
void gd_usart1_init(uint32_t baudval);
void gd_uart4_init(uint32_t baudval);

void dma_rx_config(GD_COMxTypedef com_num, uint32_t dma_buffer_addr, uint32_t dma_buff_length);
void usart1_rx_idle(uint32_t baudval, uint32_t dma_buff_addr, uint32_t dma_buff_len);
void usart0_rx_idle(uint32_t baudval, uint32_t dma_buff_addr, uint32_t dma_buff_len);
void usart_rx_idle(GD_COMxTypedef com_num, uint32_t baudval, uint32_t dma_buff_addr, uint32_t dma_buff_len);

/**
 * @brief 获取串口号
 * @param com_num 串口号, 具体为usart_typedef_enum里面的值
*/
uint32_t get_uart_num(GD_COMxTypedef com_num);

/* simple receive funtion */
GD_StatusTypeDef usart_receive_byte(uint32_t usart_periph, uint8_t* data);
GD_StatusTypeDef usart_receive(uint32_t usart_periph, uint8_t *pData, uint16_t Size);
GD_StatusTypeDef GD_UART_PutChar(uint32_t usart_periph, const uint8_t *pData, uint16_t Size);
/* IO funtion like stm32 hal */
GD_StatusTypeDef GD_UART_Transmit(uint32_t usart_periph, const uint8_t *pData, uint16_t Size, uint32_t Timeout);
GD_StatusTypeDef GD_UART_Receive(uint32_t usart_periph, uint8_t *pData, uint16_t Size, uint32_t Timeout);

/* 简单的发送函数 */
GD_StatusTypeDef UART_Transmit(uint32_t usart_periph, const uint8_t *pData, uint16_t Size);

GD_StatusTypeDef USART0_Transmit_DMA(const uint8_t pData[], uint16_t Size);
GD_StatusTypeDef USART1_Transmit_DMA(const uint8_t pData[], uint16_t Size);
