/**
 * @file        evse_comm.c
 * @brief       与LCD板通信的串口驱动
 */
#include "evse_comm.h"
#include "lwrb.h"
#include "basic_os.h"

#define LOG_TAG "evse.comm"
#include "elog.h"

#define COMM_BUFF_LENGTH   64

/* ring buffer for uart */
static lwrb_t uart_rb;
static uint8_t lwrb_buffer[COMM_BUFF_LENGTH]  = { 0x00 };
__IO static uint8_t rx_cplt_flag = false;

/* DMA缓冲区 */
uint8_t meter_buff[COMM_BUFF_LENGTH];

/**
 * @brief       启动串口DMA接收，并开启串口空闲中断；DMA开启全满和半满中断。
 */
void evse_comm_init(void)
{
    lwrb_init(&uart_rb, lwrb_buffer, COMM_BUFF_LENGTH);
    gd_usart_rx_init(COM2, 9600);
    dma_rx_config(COM2, (uint32_t)meter_buff, COMM_BUFF_LENGTH);

    usart_flag_clear(USART2, USART_FLAG_TC);
    nvic_irq_enable(USART2_IRQn, 4, 0);
    usart_interrupt_enable(USART2, USART_INT_IDLE);

    dma_flag_clear(DMA0, DMA_CH2, DMA_FLAG_HTF|DMA_FLAG_FTF);
    nvic_irq_enable(DMA0_Channel2_IRQn, 4, 0);
    dma_interrupt_enable(DMA0, DMA_CH2, DMA_CHXCTL_HTFIE|DMA_CHXCTL_FTFIE);
}


/**
 * @brief       串口空闲中断和DMA中断回调函数
 * @note        串口和DMA中断都会调用该函数, 用来处理DMA缓冲区里面的数据。
 * @param[in]   size dma缓冲区已经使用的大小
 * @param[in]   it_source 对于串口只开启超时中断或者空闲中断的应用来说, it_source可以作为帧结束的标识。
 *                  1: 表示本次是在串口空闲中断里面调用的;
 *                  0: 表示本次是在DMA中断里面调用的.
*/
void COMM_RxEventCallback(uint32_t Size, uint16_t it_source)
{
    static uint32_t dma_buf_pos = 0;    //  本次接收到的数据在dma缓冲区中的起始位置
    static uint32_t Rx_length = 0;      //  本次收到的数据长度

    Rx_length = Size - dma_buf_pos;

    lwrb_write(&uart_rb, meter_buff+dma_buf_pos, (lwrb_sz_t)Rx_length);

    if(it_source == S_UART)
        rx_cplt_flag = true; // 空闲中断或者超时中断表示一帧结束

    dma_buf_pos += Rx_length;

    if (dma_buf_pos >= COMM_BUFF_LENGTH) dma_buf_pos = 0;
}

/**
 * @brief   串口中断服务函数, DMA中断服务函数, 该函数会调用Tuya_RxEventCallback(), it_source固定传1
 * @note    计量模块的串口设备只开启了空闲中断
 * @param   none
 * @retval  none
*/
void USART2_IRQHandler(void){
    if(usart_interrupt_flag_get(USART2, USART_INT_FLAG_IDLE)){
        /* 清除 IDLE 标志位 */
        USART_STAT0(USART2);
        USART_DATA(USART2);
        /* dma缓冲区的总大小 - dma缓冲区已经使用的大小 = dma缓冲区剩余空间 */
        COMM_RxEventCallback(COMM_BUFF_LENGTH - dma_transfer_number_get(DMA0, DMA_CH2), S_UART);
    }
}

/**
 * @brief   DMA中断服务函数DMA中断服务函数, 该函数会调用Tuya_RxEventCallback()。
 * @note    由于DMA开启的是半满和全满中断, 所以在传递DMA缓冲区已经使用的大小时, 
 *          直接传递缓冲区大小的一半或者缓冲区的大小即可。 中断源固定传递0。
 * @param   none
 * @retval  none
*/
void DMA0_Channel2_IRQHandler(){
    if(dma_interrupt_flag_get(DMA0, DMA_CH2, DMA_INT_FLAG_HTF)){
        dma_interrupt_flag_clear(DMA0, DMA_CH2, DMA_INT_FLAG_HTF);
        COMM_RxEventCallback(COMM_BUFF_LENGTH/2, S_DMA);
    }
    if(dma_interrupt_flag_get(DMA0, DMA_CH2, DMA_INT_FLAG_FTF)){
        dma_interrupt_flag_clear(DMA0, DMA_CH2, DMA_INT_FLAG_FTF);
        COMM_RxEventCallback(COMM_BUFF_LENGTH, S_DMA);
    }
}

static void task_entry_comm(void *parameter)
{
    /* 初始化LCD板通信串口 */
    evse_comm_init();
    uint8_t frame[16];
    for(;;){
        if(rx_cplt_flag){
            log_i("comm");
            rx_cplt_flag = false;
            lwrb_read(&uart_rb, frame, 16);
        }else
            bos_delay_ms(1);
    }
}
bos_task_export(lcd_comm, task_entry_comm, BOS_MAX_PRIORITY, NULL);
