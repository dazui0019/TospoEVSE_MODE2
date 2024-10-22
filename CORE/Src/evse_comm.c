/**
 * @file        evse_comm.c
 * @brief       与LCD板通信的串口驱动, 任务void task_entry_comm(void *parameter)用于串口接收。
 */
#include "evse_comm.h"
#include "lwrb.h"
#include "basic_os.h"
#include "EventRecorder.h"
#include "crc16.h"
#include "fifo.h"

#define LOG_TAG "evse.comm"
#include "elog.h"

#define COMM_BUFF_LENGTH   64
/* ring buffer for uart */
static lwrb_t uart_rb;
static uint8_t lwrb_buffer[COMM_BUFF_LENGTH]  = { 0x00 };

#define SEND_BUFFER_LENGTH 256
static fifo_s_t send_fifo;
static uint8_t fifo_buffer[SEND_BUFFER_LENGTH];

/* 中断标志位 */
static __IO uint8_t rx_cplt_flag = false;

static __IO ui_update_flag_t ui_update_flag = {
    .current = false,
    .delay = false,
    .voltage = false,
    .error = false
};

/* DMA缓冲区 */
uint8_t receive_buffer[COMM_BUFF_LENGTH];

/**
 * @brief       启动串口DMA接收，并开启串口空闲中断；DMA开启全满和半满中断。
 */
void evse_comm_init(void)
{
    lwrb_init(&uart_rb, lwrb_buffer, COMM_BUFF_LENGTH);
    gd_usart_init(COM2, 9600);
    dma_rx_config(COM2, (uint32_t)receive_buffer, COMM_BUFF_LENGTH);

    fifo_s_init(&send_fifo, fifo_buffer, 256);

    usart_flag_clear(USART2, USART_FLAG_TC);
    nvic_irq_enable(USART2_IRQn, 4, 0);
    usart_interrupt_enable(USART2, USART_INT_IDLE);

    dma_flag_clear(DMA0, DMA_CH2, DMA_FLAG_HTF|DMA_FLAG_FTF);
    nvic_irq_enable(DMA0_Channel2_IRQn, 4, 0);
    dma_interrupt_enable(DMA0, DMA_CH2, DMA_CHXCTL_HTFIE|DMA_CHXCTL_FTFIE);
}

/**
 * @brief  计算校验和
 * @param[in] {pack} 数据源指针
 * @param[in] {pack_len} 计算校验和长度
 * @return 校验和
 */
uint8_t get_check_sum(uint8_t pack[], uint16_t pack_len)
{
    uint16_t i;
    uint8_t check_sum = 0;
    
    for(i = 0; i < pack_len; i ++) {
        check_sum += *pack ++;
    }
    
    return check_sum;
}

/**
 * @brief  UI更新
 * @param[in] {cmd} 功能码
 * @param[in] {arg_int} 对于不同的功能码有不同的作用
 * @param[in] {arg_ptr} 目前仅在更新电量的时候使用, 用来传递float变量指针
 */
uint8_t evse_comm_ui_update(uint8_t cmd, uint8_t arg_int, void *arg_ptr)
{
    float f_kwh;
    uint16_t s_kwh;
    /* 帧缓冲区 */
    uint8_t frame_buff[FRAME_LEN_MAX] = {0x5A}; // 帧头0x5A
    
    switch (cmd)
    {
    case FUNC_CODE_UPDATE_ERR:
        frame_buff[1] = FUNC_CODE_UPDATE_ERR;   // 功能码
        frame_buff[2] = 0x01;                   // payload长度
        frame_buff[3] = arg_int;
        frame_buff[4] = get_check_sum(frame_buff, 4);
        frame_buff[5] = 0x55;
        UART_Transmit(USART2, frame_buff, 6);
        // fifo_s_puts(&send_fifo, frame_buff, 6);
        break;
    case FUNC_CODE_UPDATE_CHG:
        frame_buff[1] = FUNC_CODE_UPDATE_CHG;   // 功能码
        frame_buff[2] = 0x01;                   // payload长度
        frame_buff[3] = arg_int;
        frame_buff[4] = get_check_sum(frame_buff, 4);
        frame_buff[5] = 0x55;
        UART_Transmit(USART2, frame_buff, 6);
        // fifo_s_puts(&send_fifo, frame_buff, 6);
        break;
    case FUNC_CODE_UPDATE_DELAY:
        frame_buff[1] = FUNC_CODE_UPDATE_DELAY; // 功能码
        frame_buff[2] = 0x01;                   // payload长度
        frame_buff[3] = arg_int;
        frame_buff[4] = get_check_sum(frame_buff, 4);
        frame_buff[5] = 0x55;
        UART_Transmit(USART2, frame_buff, 6);
        // fifo_s_puts(&send_fifo, frame_buff, 6);
        break;
    case FUNC_CODE_UPDATE_CURRENT:
        frame_buff[1] = FUNC_CODE_UPDATE_CURRENT;   // 功能码
        frame_buff[2] = 0x01;                       // payload长度
        frame_buff[3] = arg_int;
        frame_buff[4] = get_check_sum(frame_buff, 4);
        frame_buff[5] = 0x55;
        UART_Transmit(USART2, frame_buff, 6);
        // fifo_s_puts(&send_fifo, frame_buff, 6);
        break;
    case FUNC_CODE_UPDATE_KWH:
        f_kwh = *(float*)arg_ptr;
        s_kwh = (uint16_t)(f_kwh*10);
    
        frame_buff[1] = FUNC_CODE_UPDATE_KWH;   // 功能码
        frame_buff[2] = 0x02;                   // payload长度
        frame_buff[3] = ((uint16_t)s_kwh) >> 8;
        frame_buff[4] = ((uint16_t)s_kwh)&0x00FF;
        frame_buff[5] = get_check_sum(frame_buff, 5);
        frame_buff[6] = 0x55;
        UART_Transmit(USART2, frame_buff, 7);
        // fifo_s_puts(&send_fifo, frame_buff, 7);
        break;
    default:
        log_e("unknown cmd: 0x%02X.", cmd);
        break;
    }
    
    return 0;
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

    lwrb_write(&uart_rb, receive_buffer+dma_buf_pos, (lwrb_sz_t)Rx_length);

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

void evse_key_process(uint8_t key_val)
{
    switch (key_val)
    {
    case 0x01:
        /* 电流切换 */
        log_d("current switching.");
        break;
    case 0x02:
        /* 延时时间设置 */
        log_d("delay time setting.");
        break;
    case 0x03:
        /* 清除电量 */
        log_d("clearing kwh.");
        break;
    default:
        log_e("unknown key_val: 0x%02X.", key_val);
        break;
    };
}

// todo: 处理fifo中需要发送的数据
void evse_comm_send_handle(void)
{
    uint8_t frame_buff[FRAME_LEN_MAX];
    uint8_t data_temp;
    if(fifo_s_isempty(&send_fifo) == 1){return;}

    // 如果发送队列不为空则发送
    if((data_temp = fifo_s_get(&send_fifo)) == 0x5A){
        ;
    }

}

static void task_entry_comm(void *parameter)
{
    /* 初始化LCD板通信串口 */
    evse_comm_init();
    uint8_t frame[FRAME_LEN_MAX];
    uint8_t rx_length;
    for(;;){
        if(rx_cplt_flag){
            EventStartA(1);
            rx_cplt_flag = false;
            rx_length = lwrb_get_full(&uart_rb);    // 直接将ring buffer已使用的长度作为本次串口接收的长度, 不太可靠需要优化
            lwrb_read(&uart_rb, frame, rx_length);
            /* 检查帧头和帧尾 */
            if(0x5A == frame[0] &&  0x55 == frame[rx_length-1]){
                /* 检查校验位 */
                if(get_check_sum(frame, rx_length-2) != frame[rx_length-2]){ // rx_length减去校验位本身和帧尾长度
                    log_e("checksum error: 0x%02X, should be: 0x%02X.", frame[rx_length-2], get_check_sum(frame, rx_length-2));
                    continue;
                }
                log_d("code: 0x%02X.", frame[1]);
                switch (frame[1])
                {
                case 0x3A:
                    evse_key_process(frame[3]);
                    break;
                default:
                    log_e("code: 0x%02X.", frame[1]);
                    break;
                }
            }
            EventStopA(1);
            continue;
        }
        
        bos_delay_ms(1);
    }
}
bos_task_export(lcd_comm, task_entry_comm, BOS_MAX_PRIORITY, NULL);
