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
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* assert define ------------------------------------------------------------ */
#if (BOS_USE_ASSERT != 0)
#define BOS_ASSERT(test_)                                                      \
    do                                                                         \
    {                                                                          \
        if (!(test_))                                                          \
        {                                                                      \
            bos_critical_enter();                                              \
            bos_port_assert(__LINE__);                                         \
        }                                                                      \
    } while (0)
#else
#define BOS_ASSERT(test_)               ((void)0)
#endif

/* macro -------------------------------------------------------------------- */
#define BOS_MS_NUM_30DAY                (2592000000U)
#define BOS_MS_NUM_15DAY                (1296000000U)
#define BOS_STACK_MIN                   (16)        /* 16 words */

/* bos task ----------------------------------------------------------------- */
/* Basic task state */
enum
{
    BosTaskState_Ready = 0,
    BosTaskState_Running,
    BosTaskState_Blocked,
    BosTaskState_Suspended,
    BosTaskState_Stop,

    BosTaskState_Max,
};

typedef struct basic_os_tag
{
    bos_task_rom_t *task_table;
    bos_timer_rom_t *timer_table;
    uint16_t task_count;
    uint16_t timer_count;
    void *stack;
    uint16_t stack_size;
    bool timer_cb_runing;

    uint32_t time_idle_backup;
    uint32_t time;
    uint32_t time_out_min;
    uint32_t time_offset;
    uint32_t cpu_usage_count;
} basic_os_t;

/* public variables --------------------------------------------------------- */
uint32_t addr_target = 0;
uint32_t addr_source = 0;
uint32_t copy_size = 0;
uint32_t move_size = 0;

bos_task_t *volatile bos_current;
bos_task_t *volatile bos_next;

/* private variables -------------------------------------------------------- */
static basic_os_t bos;
static uint32_t stack_used = 0;

/* private function --------------------------------------------------------- */
static void bos_sheduler(void);
static void bos_start(void);
static bool bos_check_timer(bool task_idle);
static void _entry_idle(void *parameter);
static void _cb_timer_tick(void *para);

/* public function ---------------------------------------------------------- */
void bos_critical_enter(void);
void bos_critical_exit(void);
uint32_t get_sp_value(void);

/* Default task and timer --------------------------------------------------- */
/*  Note 1
    Although the priority of idle task is set to be 1. But the program in 
    bos_sheduler() makes the actual priority of idle task is 0.
*/
bos_task_export(task_timer, _entry_idle, 1, NULL);
bos_timer_export(basic_timer, _cb_timer_tick, false, NULL);

/* public function ---------------------------------------------------------- */
/**
  * @brief  BasicOS stack and tasks initialization.
  * @param  stack   The global stack memory address.
  * @param  size    The global stack memory size.
  * @retval None
  */
void basic_os_init(void *stack, uint32_t size)
{
    bos_critical_enter();
    
    bos_cpu_hw_init();

    /* Set the stack and its size. */
    uint32_t mod = (uint32_t)stack % 8;
    bos.stack = mod == 0 ? stack : (void *)((uint32_t)stack + 8 - mod);
    size = (((uint32_t)bos.stack + size - mod) / 8) * 8 - (uint32_t)bos.stack;
    bos.stack_size = size / 4;

    /* Get the task table and its counting number. */
    bos.task_table = (bos_task_rom_t *)&rom_task_task_timer;
    bos_task_rom_t *task_temp = NULL;
    while (1)
    {
        task_temp =
            (bos_task_rom_t *)((uint32_t)bos.task_table - sizeof(bos_task_rom_t));
        if (task_temp->magic_head != EXPORT_ID_TASK ||
            task_temp->magic_tail != EXPORT_ID_TASK)
        {
            break;
        }
        
        bos.task_table = task_temp;
    }
    bos.task_count = 0;
    bos.timer_cb_runing = false;
    uint32_t task_id_high_prio = 0;
    uint8_t priority = 0;
    for (uint32_t i = 0; ; i ++)
    {
        if (bos.task_table[i].magic_head == EXPORT_ID_TASK &&
            bos.task_table[i].magic_tail == EXPORT_ID_TASK)
        {
            if (bos.task_table[i].priority > priority)
            {
                task_id_high_prio = i;
                priority = bos.task_table[i].priority;
            }
            
            // TODO Check the tasks' data is not repeated.
            bos.task_count ++;
        }
        else
        {
            break;
        }
    }

    /* Get the timer table and its counting number. */
    bos.timer_table = (bos_timer_rom_t *)&tim_basic_timer;
    bos_timer_rom_t *timer_temp = NULL;
    while (1)
    {
        timer_temp =
            (bos_timer_rom_t *)((uint32_t)bos.timer_table - sizeof(bos_timer_rom_t));
        if (timer_temp->magic_head != EXPORT_ID_TIMER ||
            timer_temp->magic_tail != EXPORT_ID_TIMER)
        {
            break;
        }
        
        bos.timer_table = timer_temp;
    }
    bos.timer_count = 0;
    
    for (uint32_t i = 0; ; i ++)
    {
        if (bos.timer_table[i].magic_head == EXPORT_ID_TIMER &&
            bos.timer_table[i].magic_tail == EXPORT_ID_TIMER)
        {
            // TODO Check all timers' data is not repeated.
            bos.timer_count ++;
        }
        else
        {
            break;
        }
    }

    bos.time = 0;
    bos.time_offset = 0;
    bos.time_out_min = UINT32_MAX;
    
    /* Get the highest priority task. */
    bos_current = NULL;
    bos_next = (bos_task_t *)bos.task_table[task_id_high_prio].data;

    /* Set the stack RAM for every task. */
    uint32_t remaining = bos.stack_size - BOS_STACK_MIN * (bos.task_count - 1);
    void *stack_current = bos.stack;
    bos_task_t *task_data = NULL;
    bos_task_rom_t *task_info = NULL;
    for (uint32_t i = 0; i < bos.task_count; i ++)
    {
        task_data = (bos_task_t *)bos.task_table[i].data;
        task_data->task_id = i;
        task_info = (bos_task_rom_t *)&bos.task_table[i];
        task_data->stack_size = i == task_id_high_prio ? remaining : BOS_STACK_MIN;
        task_data->stack = stack_current;
        stack_current = (void *)((uint32_t)stack_current + task_data->stack_size * 4);

        BOS_ASSERT(task_info->priority <= BOS_MAX_PRIORITY);
        BOS_ASSERT(task_info->priority != 0);
        
        /* save the top of the stack in the task's attibute */
        task_data->sp = bos_cpu_stack_init(task_info);

        task_data->state = BosTaskState_Ready;
        task_data->state_bkp = BosTaskState_Ready;
    }

    bos_critical_exit();
}

/**
  * @brief  Start to run BasicOS kernel.
  * @retval None
  */
void basic_os_run(void)
{
    /* Run the starting hook function. */
    bos_hook_start();
    
    /* Run the BasicOS sheduler to start all the tasks. */
    bos_start();
    
    /*  It's impossible for MCU to get here. If the assert is reached, something 
        wrong occurs. */
    BOS_ASSERT(false);
}

/**
  * @brief  Get the BasicOS time in mili-second.
  * @retval BasicOS time in mili-second
  */
uint32_t bos_time(void)
{
    bos_critical_enter();
    uint32_t time_offset = bos.time_offset;
    bos_critical_exit();

    return (time_offset + bos.time);
}

/**
  * @brief  The BasicOS tick function. Please put it into one timer ISR and set
  *         the BOS_TICK_MS macro to the correct value.
  * @retval None.
  */
void bos_tick(void)
{
    bos_critical_enter();
    bos.time += BOS_TICK_MS;
    bos_critical_exit();
}

/**
  * @brief  The BasicOS delay function in the current thread.
  * @param  time_ms     Delayed time in mili-seconds.
  * @note   It can NOT be used in the Idle hook function.
  * @retval None.
  */
void bos_delay_ms(uint32_t time_ms)
{
    BOS_ASSERT(!bos.timer_cb_runing);

    if (time_ms == 0)
    {
        bos_task_yield();
        return;
    }
    
    BOS_ASSERT(time_ms <= BOS_MS_NUM_30DAY);

    /* Never call bos_delay_ms in the idle task. */
    BOS_ASSERT(bos_current != &ram_task_timer_data);
    
    bos_task_t *task_data = NULL;
    uint32_t count = 0;
    bos_critical_enter();
    bos_current->timeout = bos.time + time_ms;
    bos_current->state = BosTaskState_Blocked;
    for (uint32_t i = bos_current->task_id;;)
    {
        task_data = (bos_task_t *)bos.task_table[i].data;
        if (task_data != bos_current &&
            bos.task_table[i].priority == bos.task_table[bos_current->task_id].priority &&
            (task_data->state == BosTaskState_Suspended ||
            task_data->state == BosTaskState_Ready))
        {
            task_data->state = BosTaskState_Ready;
            break;
        }

        count ++;
        i = (i + 1) % bos.task_count;
        if (count >= bos.task_count)
        {
            break;
        }
    }
    bos_critical_exit();
    
    bos_sheduler();
}

/**
  * @brief  The BasicOS terminate the current thread.
  * @retval None.
  */
void bos_task_exit(void)
{
    bos_critical_enter();
    bos_current->state = BosTaskState_Stop;
    bos_critical_exit();
    
    bos_sheduler();
}

/**
  * @brief  The function is used to request a context switch to another task. 
  *         However, if there are no other tasks at a higher or equal priority 
  *         to the task that calls bos_task_yield() then the RTOS scheduler will
  *         simply select the task that called bos_task_yield() to run again.
  * @retval None.
  */
void bos_task_yield(void)
{
    bos_task_t *task_data = NULL;

    bos_check_timer(false);
    
    bos_critical_enter();
    
    /* Find the next task in the same priority with the current task. */
    uint8_t priority_current = bos.task_table[bos_current->task_id].priority;
    bool found = false;
    uint32_t count = 0;
    for (uint32_t i = bos_current->task_id;;)
    {
        task_data = (bos_task_t *)bos.task_table[i].data;
        if (task_data != bos_current &&
            bos.task_table[i].priority == priority_current &&
            (task_data->state == BosTaskState_Suspended ||
            task_data->state == BosTaskState_Ready))
        {
            task_data->state = BosTaskState_Ready;
            bos_current->state = BosTaskState_Suspended;
            found = true;
            break;
        }

        count ++;
        i = (i + 1) % bos.task_count;
        if (count >= bos.task_count)
        {
            break;
        }
    }
    bos_critical_exit();

    if (found)
    {
        bos_sheduler();
    }
}

/* Soft timer --------------------------------------------------------------- */
/**
  * @brief  Get the BasicOS timer's ID from its name.
  * @retval Timer ID when positive or error id when negetive.
  */
int16_t bos_timer_get_id(const char *name)
{
    /* Find the timer in the task table. */
    int16_t ret = BOS_NOT_FOUND;
    for (uint32_t i = 0; i < bos.timer_count; i ++)
    {
        if (strcmp(bos.timer_table[i].name, name) == 0)
        {
            ret = i;
            break;
        }
    }

    return ret;
}

/**
  * @brief  Start one soft timer exported by bos_timer_export.
  * @param  timer_id    The timer ID.
  * @param  period      The soft-timer's period in mili-second.
  * @retval None.
  */
void bos_timer_start(uint16_t timer_id, uint32_t period)
{
    BOS_ASSERT(timer_id < bos.timer_count);
    BOS_ASSERT(period <= BOS_MS_NUM_30DAY);

    bos_critical_enter();

    bos_timer_t *timer = (bos_timer_t *)bos.timer_table[timer_id].data;
    timer->running = 1;
    timer->period = period;

    timer->timeout = bos.time + period;
    if (timer->timeout < bos.time_out_min)
    {
        bos.time_out_min = timer->timeout;
    }
    

    bos_critical_exit();
}

/**
  * @brief  Stop the soft timer exported by bos_timer_export.
  * @param  timer_id    The timer ID.
  * @retval None.
  */
void bos_timer_stop(uint16_t timer_id)
{
    BOS_ASSERT(timer_id < bos.timer_count);
    bos_timer_t *timer = NULL;
    uint32_t time_out_min = UINT32_MAX;

    bos_critical_enter();

    for (uint32_t i = 0; i < bos.timer_count; i ++)
    {
        timer = (bos_timer_t *)bos.timer_table[timer_id].data;
        if (i == timer_id)
        {
            timer->running = 0;
        }
        else if (timer->running != 0 && time_out_min > timer->timeout)
        {
            time_out_min = timer->timeout;
        }
    }
    bos.time_out_min = time_out_min;

    bos_critical_exit();
}

/**
  * @brief  Re-start one soft timer exported by bos_timer_export.
  * @param  timer_id    The timer ID.
  * @param  period      The soft-timer's period in mili-second.
  * @retval None.
  */
void bos_timer_reset(uint16_t timer_id, uint32_t period)
{
    BOS_ASSERT(timer_id < bos.timer_count);
    BOS_ASSERT(period <= BOS_MS_NUM_30DAY);

    bos_critical_enter();

    bos_timer_t *timer = (bos_timer_t *)bos.timer_table[timer_id].data;
    timer->running = 1;
    timer->period = period;
    if (bos.time_out_min > timer->timeout)
    {
        bos.time_out_min = timer->timeout;
    }

    bos_critical_exit();
}

/**
  * @brief  Estimate the used stack size in the BasicOS kernel.
  * @retval The used stack size.
  */
uint32_t bos_get_used_stack_size(void)
{
    return stack_used;
}

/* private function --------------------------------------------------------- */
/**
  * @brief  Check all thread timers and soft-timers are timeout or not.
  * @param  task_idle   In idle thread or not.
  * @retval If false, not timer is timeout.
  */
static bool bos_check_timer(bool task_idle)
{
    bool ret = false;
    bos_timer_t *timer_data = NULL;
    bos_task_t *task_data = NULL;
    
    if (bos.time_idle_backup != bos.time)
    {
        bos.time_idle_backup = bos.time;
        
        bos_critical_enter();

        /* check all the task are timeout or not. */
        bool task_timeout = false;
        for (uint32_t i = 0; i < bos.task_count; i ++)
        {
            task_data = (bos_task_t *)bos.task_table[i].data;
            if (task_data->state == BosTaskState_Blocked)
            {
                if (bos.time >= task_data->timeout)
                {
                    task_data->state = BosTaskState_Ready;
                    task_timeout = true;
                }
            }
        }
        if (task_idle && task_timeout)
        {
            bos_critical_exit();
            bos_sheduler();
            bos_critical_enter();
        }
        
        if (bos.time >= BOS_MS_NUM_15DAY)
        {
            /* Adjust all tasks' timing. */
            for (uint32_t i = 0; i < bos.task_count; i ++)
            {
                task_data = (bos_task_t *)bos.task_table[i].data;
                if (task_data->state == BosTaskState_Blocked)
                {
                    task_data->timeout -= bos.time;
                }
            }

            /* Adjust all timers' timing. */
            for (uint32_t i = 0; i < bos.timer_count; i ++)
            {
                timer_data = (bos_timer_t *)bos.timer_table[i].data;
                if (timer_data->running != 0)
                {
                    timer_data->timeout -= bos.time;
                }
            }

            bos.time_out_min -= bos.time;
            bos.time_offset += bos.time;
            bos.time = 0;
        }

        /* if any timer is timeout */
        if (bos.time >= bos.time_out_min)
        {
            /* Find the time-out timers and excute the handlers. */
            for (uint32_t i = 0; i < bos.timer_count; i ++)
            {
                timer_data = (bos_timer_t *)bos.timer_table[i].data;
                if (timer_data->running != 0 && bos.time >= timer_data->timeout)
                {
                    bos.timer_cb_runing = true;
                    bos_critical_exit();
                    bos.timer_table[i].func(bos.timer_table[i].parameter);
                    ret = true;
                    bos_critical_enter();
                    bos.timer_cb_runing = false;
                    if (bos.timer_table[i].oneshoot == 0)
                    {
                        timer_data->timeout += timer_data->period;
                    }
                    else
                    {
                        timer_data->running = 0;
                    }
                }
            }

            /* Recalculate the minimum timeout value. */
            if (ret)
            {
                uint32_t time_out_min = UINT32_MAX;
                for (uint32_t i = 0; i < bos.timer_count; i ++)
                {
                    timer_data = (bos_timer_t *)bos.timer_table[i].data;
                    if (timer_data->running != 0 && time_out_min >= timer_data->timeout)
                    {
                        time_out_min = timer_data->timeout;
                    }
                }
                bos.time_out_min = time_out_min;
            }
        }

        bos_critical_exit();
    }
    
    return ret;
}

/**
  * @brief  Start BasicOS sheduler.
  * @retval None.
  */
static void bos_start(void)
{
    bos_critical_enter();
    bos_cpu_trig_task_switch();
    bos_critical_exit();
}

/**
  * @brief  BasicOS sheduler.
  * @retval None.
  */
static void bos_sheduler(void)
{
    bos_task_t *task_data = NULL;

    bos_critical_enter();

    /* The actual priority of idle task is 0. */
    bos_next = &ram_task_timer_data;
    uint8_t priority = 0;
    uint32_t count = 0;
    for (uint32_t i = ((bos_current->task_id + 1) % bos.task_count); ;)
    {
        task_data = (bos_task_t *)bos.task_table[i].data;
        if (task_data->state == BosTaskState_Ready &&
            bos.task_table[i].priority > priority)
        {
            bos_next = task_data;
            priority = bos.task_table[i].priority;
        }

        if (task_data->state == BosTaskState_Ready &&
            bos.task_table[i].priority == bos.task_table[bos_current->task_id].priority &&
            bos_next == &ram_task_timer_data)
        {
            bos_next = task_data;
            priority = bos.task_table[i].priority;
        }

        count ++;
        i = (i + 1) % bos.task_count;
        if (count >= bos.task_count)
        {
            break;
        }
    }
    
    /* Switch the task */
    if (bos_next != bos_current)
    {
        #define STACK_SIZE_PUSH                 (64)
        
        uint32_t sp_value = get_sp_value();
        uint32_t _stack_used = (bos.task_count << 7) + (bos.stack_size << 2) -
                                (sp_value - (uint32_t)bos_current->stack);
        stack_used = (_stack_used > stack_used) ? _stack_used : stack_used;
        
        /* The current task move to front. */
        if (bos_next->task_id < bos_current->task_id)
        {
            copy_size = bos_next->stack_size << 2;
            move_size = sp_value - STACK_SIZE_PUSH - (uint32_t)bos_current->stack;
            for (uint32_t i = bos_next->task_id + 1; i < bos_current->task_id; i ++)
            {
                task_data = (bos_task_t *)bos.task_table[i].data;
                task_data->stack = (void *)((uint32_t)task_data->stack + move_size);
                task_data->sp = (void *)((uint32_t)task_data->sp + move_size);
                copy_size += task_data->stack_size << 2;
            }
            addr_target = (uint32_t)bos_next->stack + move_size;
            addr_source = (uint32_t)bos_next->stack;
            
            bos_current->stack = (void *)((uint32_t)bos_current->stack + move_size);
            bos_current->sp = (void *)((uint32_t)sp_value - STACK_SIZE_PUSH);
            
            bos_current->stack_size -= (move_size >> 2);
            bos_next->stack_size += (move_size >> 2);
            bos_next->sp = (void *)((uint32_t)bos_next->sp + move_size);
            move_size = move_size;
        }
        /* The current task move to back. */
        else
        {
            move_size = sp_value - STACK_SIZE_PUSH - (uint32_t)bos_current->stack;
            copy_size = (bos_current->stack_size << 2) - move_size;
            addr_target = (uint32_t)bos_current->stack;
            addr_source = (uint32_t)(sp_value - STACK_SIZE_PUSH);
            for (uint32_t i = bos_current->task_id + 1; i < bos_next->task_id; i ++)
            {
                task_data = (bos_task_t *)bos.task_table[i].data;
                task_data->stack = (void *)((uint32_t)task_data->stack - move_size);
                task_data->sp = (void *)((uint32_t)task_data->sp - move_size);
                copy_size += task_data->stack_size << 2;
            }
            
            bos_current->stack_size -= (move_size >> 2);
            bos_next->stack_size += (move_size >> 2);
            bos_current->sp = bos_current->stack;
            bos_next->stack = (void *)((uint32_t)bos_next->stack - move_size);
            move_size = move_size;
        }

        #undef STACK_SIZE_PUSH
        
        bos_cpu_trig_task_switch();
    }

    bos_critical_exit();
}

/**
  * @brief  The idle task entry function.
  * @param  parameter   The idle task parameter.
  * @retval None.
  */
static void _entry_idle(void *parameter)
{
    (void)parameter;
    
    while (1)
    {
        /* If no timer is timeout. */
        if (!bos_check_timer(true))
        {
            bos_hook_idle();
        }
    }
}

/**
  * @brief  Callback function if the timer.
  * @param  para    The idle task parameter.
  * @retval None.
  */
static void _cb_timer_tick(void *para)
{
}

#ifdef __cplusplus
}
#endif

/* ----------------------------- end of file -------------------------------- */
