
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
 * 2022-03-21     GouGe         V0.1.0
 * 2023-04-22     GouGe         V0.2.0
 */

#ifndef BASIC_OS_H_
#define BASIC_OS_H_

/* include ------------------------------------------------------------------ */
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Public config ------------------------------------------------------------ */
/**
  * @brief  The maximum number of tasks in BasicOS.
  */
#define BOS_MAX_TASKS                           (16)

/**
  * @brief  The maximum priority of tasks in BasicOS.
  */
#define BOS_MAX_PRIORITY                        (8)

/**
  * @brief  The tick period time in mili-second.
  */
#define BOS_TICK_MS                             (1)

/**
  * @brief  Basic assert function configuration.
  */
#define BOS_USE_ASSERT                          (1)

/**
  * @brief  Basic stack usage function configuration.
  */
#define BOS_USE_STACK_USAGE                     (0)

/**
  * @brief  Basic cpu usage function configuration.
  */
#define BOS_USE_CPU_USAGE                       (0)

/* Data structure ----------------------------------------------------------- */
enum bos_error
{
    BOS_OK                          = 0,
    BOS_NOT_FOUND                   = -1,
};

typedef void (* bos_func_t)(void *parameter);

typedef struct bos_task_rom
{
    uint32_t magic_head;
    bos_func_t func;
    uint32_t priority;
    const char *name;
    void *parameter;
    void *data;
    uint32_t magic_tail;
} bos_task_rom_t;

typedef struct bos_timer_rom
{
    uint32_t magic_head;
    bos_func_t func;
    const char *name;
    void *parameter;
    void *data;
    bool oneshoot;
    uint32_t magic_tail;
} bos_timer_rom_t;

/* Task related. */
typedef struct eos_task
{
    void *sp;
    void *stack;
    uint32_t timeout;
    uint32_t stack_size             : 16;
    uint32_t state                  : 4;
    uint32_t state_bkp              : 4;
    uint32_t task_id                : 8;
} bos_task_t;

/* Timer related. */
typedef struct eos_timer
{
    uint32_t timeout;
    uint32_t period;
    uint32_t id                     : 10;
    uint32_t domain                 : 8;
    uint32_t running                : 1;
} bos_timer_t;

/* Task --------------------------------------------------------------------- */
/**
  * @brief  BasicOS stack and tasks initialization.
  * @param  stack   The global stack memory address.
  * @param  size    The global stack memory size.
  * @retval None
  */
void basic_os_init(void *stack, uint32_t size);

/**
  * @brief  Start to run BasicOS kernel.
  * @retval None
  */
void basic_os_run(void);

/**
  * @brief  Get the BasicOS time in mili-second.
  * @retval BasicOS time in mili-second
  */
uint32_t bos_time(void);

/**
  * @brief  The BasicOS tick function. Please put it into one timer ISR and set
  *         the BOS_TICK_MS macro to the correct value.
  * @retval None.
  */
void bos_tick(void);

/**
  * @brief  The BasicOS delay function in the current thread.
  * @param  time_ms     Delayed time in mili-seconds.
  * @note   It can NOT be used in the Idle hook function.
  * @retval None.
  */
void bos_delay_ms(uint32_t time_ms);

/**
  * @brief  The BasicOS terminate the current thread.
  * @retval None.
  */
void bos_task_exit(void);

/**
  * @brief  The function is used to request a context switch to another task. 
  *         However, if there are no other tasks at a higher or equal priority 
  *         to the task that calls bos_task_yield() then the RTOS scheduler will
  *         simply select the task that called bos_task_yield() to run again.
  * @retval None.
  */
void bos_task_yield(void);

/**
  * @brief  Estimate the used stack size in the BasicOS kernel.
  * @retval The used stack size.
  */
uint32_t bos_get_used_stack_size(void);

/* Soft timer --------------------------------------------------------------- */
/**
  * @brief  Get the BasicOS timer's ID from its name.
  * @retval Timer ID when positive or error id when negetive.
  */
int16_t bos_timer_get_id(const char *name);

/**
  * @brief  Start one soft timer exported by bos_timer_export.
  * @param  timer_id    The timer ID.
  * @param  period      The soft-timer's period in mili-second.
  * @retval None.
  */
void bos_timer_start(uint16_t timer_id, uint32_t period);

/**
  * @brief  Stop the soft timer exported by bos_timer_export.
  * @param  timer_id    The timer ID.
  * @retval None.
  */
void bos_timer_stop(uint16_t timer_id);

/**
  * @brief  Re-start one soft timer exported by bos_timer_export.
  * @param  timer_id    The timer ID.
  * @param  period      The soft-timer's period in mili-second.
  * @retval None.
  */
void bos_timer_reset(uint16_t timer_id, uint32_t period);

/* Export ------------------------------------------------------------------- */
/**
  * @brief  Export one BasicOS task.
  * @param  _name       The task name.
  * @param  _func       The task entry function.
  * @param  _priority   The task priority.
  * @param  para        The task paramter.
  * @retval None.
  */
#define bos_task_export(_name, _func, _priority, para)                         \
    static bos_task_t ram_##_name##_data;                                      \
    BOS_USED const bos_task_rom_t rom_task_##_name BOS_SECTION("task_rom") =   \
    {                                                                          \
        .name = #_name,                                                        \
        .func = _func,                                                         \
        .priority = (uint32_t)_priority,                                       \
        .parameter = para,                                                     \
        .data = &ram_##_name##_data,                                           \
        .magic_head = EXPORT_ID_TASK,                                          \
        .magic_tail = EXPORT_ID_TASK,                                          \
    }

/**
  * @brief  Export one BasicOS timer.
  * @param  _name       The timer name.
  * @param  _func       The timer callback function.
  * @param  _oneshoot   The task is oneshoot or not.
  * @param  para        The timer paramter.
  * @retval None.
  */
#define bos_timer_export(_name, _func, _oneshoot, _para)                       \
    static bos_timer_t timer_##_name##_data;                                   \
    BOS_USED const bos_timer_rom_t tim_##_name BOS_SECTION("timer_rom") =      \
    {                                                                          \
        .name = (const char *)#_name,                                          \
        .func = _func,                                                         \
        .oneshoot = _oneshoot,                                                 \
        .parameter = _para,                                                    \
        .data = (void *)&timer_##_name##_data,                                 \
        .magic_head = EXPORT_ID_TIMER,                                         \
        .magic_tail = EXPORT_ID_TIMER,                                         \
    }

/* CMSIS RTOS API v2--------------------------------------------------------- */
#define osKernelInitialize                  basic_os_init
#define osKernelStart                       basic_os_run
#define osDelay                             bos_delay_ms

/* port --------------------------------------------------------------------- */
void bos_cpu_hw_init(void);
void* bos_cpu_stack_init(bos_task_rom_t *task_info);
void bos_cpu_trig_task_switch(void);

/* hook --------------------------------------------------------------------- */
/* The idle hook function. */
void bos_hook_idle(void);

/* The hook function when BasicOS starts. */
void bos_hook_start(void);

void bos_port_assert(uint32_t error_id);

/* else --------------------------------------------------------------------- */
#if (BOS_MAX_TASKS > 32)
#error The total number of tasks in BasicOS can NOT be larger than 32 !
#endif

#define EXPORT_ID_TASK                          (0xa5a5a5a5)
#define EXPORT_ID_TIMER                         (0xbeefbeef)

/* Compiler Related Definitions */
#if defined(__CC_ARM) || defined(__CLANG_ARM) /* ARM Compiler */
    #include <stdarg.h>
    #define BOS_SECTION(x)              __attribute__((section(x)))
    #define BOS_USED                    __attribute__((used))

#elif defined (__IAR_SYSTEMS_ICC__)           /* for IAR Compiler */
    #include <stdarg.h>
    #define BOS_SECTION(x)              @ x
    #define BOS_USED                    __root

#elif defined (__GNUC__)                      /* GNU GCC Compiler */
    #include <stdarg.h>
    #define BOS_SECTION(x)              __attribute__((section(x)))
    #define BOS_USED                    __attribute__((used))
#else
    #error The compiler is not supported by BasicOS !
#endif

#ifdef __cplusplus
}
#endif

#endif /* BASIC_OS_H_ */

/* ----------------------------- end of file -------------------------------- */
