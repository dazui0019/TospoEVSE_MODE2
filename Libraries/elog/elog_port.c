/*
 * This file is part of the EasyLogger Library.
 *
 * Copyright (c) 2015, Armink, <armink.ztl@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * 'Software'), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Function: Portable interface for non-os stm32f10x.
 * Created on: 2015-04-28
 */
#define LOG_TAG    "elog_port"
#include "elog.h"
#include "drv_uart.h"
#include "basic_os.h"
#include "printf.h"
// #include <stdio.h>
#include "SEGGER_RTT.h"

static void elog_print(const char *log, size_t size);

uint32_t out_lock = false;

#if defined(__RTOS)
extern SemaphoreHandle_t elog_lockHandle;      /* 给elog使用 */
extern SemaphoreHandle_t elog_dma_lockHandle;  /* 给elog 串口发送dma使用 */
#endif

/**
 * EasyLogger port initialize
 *
 * @return result
 */
ElogErrCode elog_port_init(void) {
    ElogErrCode result = ELOG_NO_ERR;
    
    // debug_init();
    // rcu_periph_clock_enable(RCU_DMA0);
    /* set EasyLogger log format */
    elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_ALL);
    elog_set_fmt(ELOG_LVL_ERROR, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
    elog_set_fmt(ELOG_LVL_WARN, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
    elog_set_fmt(ELOG_LVL_INFO, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
    elog_set_fmt(ELOG_LVL_DEBUG, ELOG_FMT_ALL & ~(ELOG_FMT_T_INFO | ELOG_FMT_P_INFO));
    elog_set_fmt(ELOG_LVL_VERBOSE, ELOG_FMT_ALL & ~(ELOG_FMT_FUNC | ELOG_FMT_T_INFO | ELOG_FMT_P_INFO));
    
    return result;
}

/**
 * EasyLogger port deinitialize
 *
 * @return result
 */
ElogErrCode elog_port_deinit(void) {
    ElogErrCode result = ELOG_NO_ERR;

    return result;
}

/**
 * output log port interface
 *
 * @param log output of log
 * @param size log size
 */
void elog_port_output(const char *log, size_t size) {
    // elog_print(log, size);
    // UART_Transmit(USART0, (uint8_t *)log, size);
    // xSemaphoreTake(elog_dma_lockHandle, portMAX_DELAY);
    //TODO output to flash
    /* output to terminal */
    // SEGGER_RTT_printf(0, "%.*s", size, log);
    SEGGER_RTT_Write(0, log, size);
}

/**
 * output lock
 */
void elog_port_output_lock(void) {
    while (out_lock != false){ // 等待上一次输出完成
        bos_delay_ms(1);
    }
    out_lock = true;
}

/**
 * output unlock
 */
void elog_port_output_unlock(void) {
    out_lock = false;
}

/**
 * get current time interface
 *
 * @return current time
 */
const char *elog_port_get_time(void) {
    static char cur_system_time[16] = "";
    snprintf(cur_system_time, 16, "%u", bos_time()); // get basic os ticks
    return cur_system_time;
}

/**
 * get current process name interface
 *
 * @return current process name
 */
const char *elog_port_get_p_info(void) {
    return "";
}

/**
 * get current thread name interface
 *
 * @return current thread name
 */
const char *elog_port_get_t_info(void) {
    // return pcTaskGetName(NULL);
    return "";
}

#ifdef __RTOS
void elog_print(const char *log, size_t size)
{
    usart_flag_clear(USART1, USART_FLAG_TC);
    dma_parameter_struct dma_init_struct;
    /* enable DMA0 */
    usart_dma_transmit_config(USART1, USART_TRANSMIT_DMA_DISABLE);
    dma_channel_disable(DMA0, DMA_CH6);
    dma_flag_clear(DMA0, DMA_CH6, DMA_INT_FLAG_FTF|DMA_INT_FLAG_HTF);
    /* deinitialize DMA channel3(USART1 tx) */
    dma_deinit(DMA0, DMA_CH6);
    dma_init_struct.direction = DMA_MEMORY_TO_PERIPHERAL;
    dma_init_struct.memory_addr = (uint32_t)log;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_init_struct.number = size;
    dma_init_struct.periph_addr = (uint32_t)&USART_DATA(USART1);
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_init(DMA0, DMA_CH6, &dma_init_struct);
    /* configure DMA mode */
    dma_circulation_disable(DMA0, DMA_CH6);
    dma_memory_to_memory_disable(DMA0, DMA_CH6);

    dma_interrupt_enable(DMA0, DMA_CH6, DMA_INT_FTF);
    nvic_irq_enable(DMA0_Channel6_IRQn, 5, 0);
    
    /* 启动一次传输 */
    dma_channel_enable(DMA0, DMA_CH6);
    usart_dma_transmit_config(USART1, USART_TRANSMIT_DMA_ENABLE);
}

void DMA0_Channel6_IRQHandler(void)
{
    dma_interrupt_flag_clear(DMA0, DMA_CH6, DMA_INT_FLAG_FTF);
    if(NULL != elog_dma_lockHandle){
        xSemaphoreGiveFromISR(elog_dma_lockHandle, NULL);
    }
}
#endif /* __RTOS */