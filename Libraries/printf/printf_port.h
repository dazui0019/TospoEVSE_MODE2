#pragma once

#include "drv_uart.h"
#include "printf.h"
#include "stdint.h"

/**
 * @brief       提供给printf.c的接口函数, 在串口重定向时使用
 * @param[in]   character 需要发送的字符
 * @retval      none
*/
void _putchar(char character);

/**
 * @brief       将printf重定向到指定的串口号上
 * @param[in]   com_num 串口号
 * @retval      none
 */
void retarget_printf(GD_COMxTypedef com_num);
