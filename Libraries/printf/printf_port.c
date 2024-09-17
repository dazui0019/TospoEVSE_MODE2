#include "printf_port.h"

static uint32_t pUART = 0;       // printf重定向使用

void _putchar(char character)
{
    USART_DATA(pUART) = USART_DATA_DATA & character;
    while(RESET == (USART_REG_VAL(pUART, USART_FLAG_TBE) & BIT(USART_BIT_POS(USART_FLAG_TBE))));
}

void retarget_printf(GD_COMxTypedef com_num)
{
    pUART = get_uart_num(com_num);
}
