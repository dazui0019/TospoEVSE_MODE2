/*
 * BasicOS V0.2
 * Copyright (c) 2021, EventOS Team, <event-os@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the 'Software'), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
 * copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS 
 * OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 * IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.event-os.cn
 * https://github.com/event-os/eventos-basic
 * https://gitee.com/event-os/eventos-basic
 * 
 * Change Logs:
 * Date           Author        Notes
 * 2021-11-23     GouGe         V0.1.0
 * 2023-04-23     GouGe         V0.2.0
 */

/* include ------------------------------------------------------------------ */
#include "basic_os.h"

/* public function ---------------------------------------------------------- */
void bos_cpu_hw_init(void)
{
    /* Set PendSV to be the lowest priority. */
    *(uint32_t volatile *)0xE000ED20 |= (0xFFU << 16U);
}

void* bos_cpu_stack_init(bos_task_rom_t *task_info)
{
    bos_task_t *task_data = (bos_task_t *)task_info->data;

    /* round down the stack top to the 8-byte boundary
        * NOTE: ARM Cortex-M stack grows down from hi -> low memory
        */
    uint32_t *sp = (uint32_t *)((uint32_t)task_data->stack + task_data->stack_size * 4);

    *(-- sp) = (uint32_t)(1 << 24);            /* xPSR, Set Bit24(Thumb Mode) to 1. */
    *(-- sp) = (uint32_t)task_info->func;      /* the entry function (PC) */
    *(-- sp) = (uint32_t)task_info->func;      /* R14(LR) */
    *(-- sp) = (uint32_t)0x12121212u;          /* R12 */
    *(-- sp) = (uint32_t)0x03030303u;          /* R3 */
    *(-- sp) = (uint32_t)0x02020202u;          /* r2 */
    *(-- sp) = (uint32_t)0x01010101u;          /* R1 */
    *(-- sp) = (uint32_t)task_info->parameter; /* r0 */

    /* additionally, fake registers r4-r11 */
    *(-- sp) = (uint32_t)0x11111111u;          /* r11 */
    *(-- sp) = (uint32_t)0x10101010u;          /* r10 */
    *(-- sp) = (uint32_t)0x09090909u;          /* r9 */
    *(-- sp) = (uint32_t)0x08080808u;          /* r8 */
    *(-- sp) = (uint32_t)0x07070707u;          /* r7 */
    *(-- sp) = (uint32_t)0x06060606u;          /* r6 */
    *(-- sp) = (uint32_t)0x05050505u;          /* r5 */
    *(-- sp) = (uint32_t)0x04040404u;          /* r4 */

    return sp;
}

void bos_cpu_trig_task_switch(void)
{
    /* Trig task switching. */
    *(uint32_t volatile *)0xE000ED04 = (1U << 28);
}

/* ----------------------------- end of file -------------------------------- */
