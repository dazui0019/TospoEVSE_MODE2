#include "evse_key.h"
#include "printf.h"
#include "drv_delay.h"
#include "basic_os.h"
#include "EventRecorder.h"
#include "evse_charge.h"
#include "gd32f303x_start.h"
#include "evse_timer.h"
#include "evse_cfg.h"

#define LOG_TAG "evse.key"
#include "elog.h"

uint8_t Key0_isPressed = false;
uint8_t Key1_isPressed = false;
uint8_t Key2_isPressed = false;
uint8_t Key3_isPressed = false;

static evse_state_t evse_state;

static void task_entry_key_test(void *parameter)
{
    for(;;){
        if(Key0_isPressed == false){
            Key0_isPressed = true;
        }
        bos_delay_ms(100);
    }
}
// bos_task_export(key_test, task_entry_key_test, BOS_MAX_PRIORITY, NULL);

static void task_entry_key_scan(void *parameter)
{
    // 只有电流改变时，才需要写入闪存
    __IO uint8_t cur_changed = false;
    // 最后一次按键按下的时间, 用于判断何时写入闪存
    __IO uint32_t tick_last = 0;
    // 当前时间
    __IO uint32_t tick_now = 0;

    // 等待充电桩启动
    while (evse_get_state() == EVSE_REBOOT)
    {
        bos_delay_ms(10);
    }
    
    for(;;){
        if(Key2_isPressed){
            tick_last = bos_time();
            if(Key3_isPressed == true){
                Key2_isPressed = false;
                Key3_isPressed = false;
                continue;
            }
            evse_state = evse_get_state();
            if(EVSE_IDLE == evse_state || EVSE_WAIT_PLUGIN == evse_state || EVSE_9V == evse_state){
                log_d("Key2 is pressed!");
                evse_max_current_switch();
                cur_changed = true;
            }else{
                log_d("Busy.");
                cur_changed = false;
            }
            Key2_isPressed = false;
            continue;
        }
        if(Key3_isPressed){
            tick_last = bos_time();
            if(Key2_isPressed == true){
                Key2_isPressed = false;
                Key3_isPressed = false;
                continue;
            }
            evse_state = evse_get_state();
            if(EVSE_IDLE == evse_state || EVSE_WAIT_PLUGIN == evse_state || EVSE_9V == evse_state){
                log_d("Key3 is pressed!");
                evse_delay_inc();
            }else{
                log_d("Busy.");
            }
            Key3_isPressed = false;
            continue;   // beep就当延时了。所以跳过后面大循环的 bos_delay_ms(10);
        }

        /* 按键按下1秒后，并且电流改变时，将电流值写入闪存 */
        if(cur_changed){
            tick_now = bos_time();
            if(tick_now > (tick_last + 1000)){
                log_d("write to flash.");
                cur_changed = false;
                evse_max_current_save();
            }else if((tick_now < tick_last) && (0xFFFFFFFF - tick_last + tick_now) > 1000){ // 溢出处理
                log_d("tick overflow.");
                log_d("write to flash.");
                cur_changed = false;
                evse_max_current_save();
            }
        }
        bos_delay_ms(50);
    }
}
bos_task_export(key_scan, task_entry_key_scan, BOS_MAX_PRIORITY, NULL);

void evse_key_init(void)
{
    gd_key_init(KEY2, KEY_MODE_EXTI);
    gd_key_init(KEY3, KEY_MODE_EXTI);
}

uint32_t Key2_StartTick = 0;    //记录上升沿中断触发时的Tick
uint32_t Key2_StopTick = 0;     //记录下降沿中断触发时的Tick
uint32_t Key3_StartTick = 0;    //记录上升沿中断触发时的Tick
uint32_t Key3_StopTick = 0;     //记录下降沿中断触发时的Tick
void EXTI10_15_IRQHandler(void)
{
    if(RESET != exti_interrupt_flag_get(EXTI_11)) {
        if(SET == gpio_input_bit_get(KEY2_GPIO_PORT, KEY2_PIN)){ Key2_StartTick = getTick(); } // 记录上升沿时刻的Tick值
        else if(RESET == gpio_input_bit_get(KEY2_GPIO_PORT, KEY2_PIN)){ // 按键释放后，才算一次完整的按键输入
            Key2_StopTick = getTick();
            if(SET == gpio_input_bit_get(KEY3_GPIO_PORT, KEY3_PIN)){return;} // 防止两个按键(Key0和Key1)同时按下
            if(Key2_StopTick > (Key2_StartTick+50)){
                Key2_isPressed = true;
            }
        }
        exti_interrupt_flag_clear(EXTI_11);
    }else if(RESET != exti_interrupt_flag_get(EXTI_12)) {
        if(SET == gpio_input_bit_get(KEY3_GPIO_PORT, KEY3_PIN)){ Key3_StartTick = getTick(); } // 记录上升沿时刻的Tick值
        else if(RESET == gpio_input_bit_get(KEY3_GPIO_PORT, KEY3_PIN)){ // 按键释放后，才算一次完整的按键输入
            Key3_StopTick = getTick();
            if(SET == gpio_input_bit_get(KEY3_GPIO_PORT, KEY3_PIN)){return;} // 防止两个按键(Key0和Key1)同时按下
            if(Key3_StopTick > (Key3_StartTick+50)){
                Key3_isPressed = true;
            }
        }
        exti_interrupt_flag_clear(EXTI_12);
    }
}
