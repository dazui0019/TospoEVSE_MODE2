#include "evse_key.h"
#include "printf.h"
#include "drv_delay.h"
#include "basic_os.h"
#include "EventRecorder.h"

#include "gd32f303x_start.h"

void evse_key_init(void);

uint8_t Key0_isPressed = false;
uint8_t Key1_isPressed = false;

static void task_entry_key_scan(void *parameter)
{
    evse_key_init();
    gd_led_init(LED2);
    for(;;){
        if(Key0_isPressed){
            printf("Key0 is pressed!\n");
            gd_led_on(LED2);
            Key0_isPressed = false;
        }
        if(Key1_isPressed){
            printf("Key1 is pressed!\n");
            gd_led_off(LED2);
            Key1_isPressed = false;
        }
        bos_delay_ms(1);
    }
}
// bos_task_export(key_scan, task_entry_key_scan, BOS_MAX_PRIORITY, NULL);

void evse_key_init(void)
{
    gd_key_init(KEY0, KEY_MODE_EXTI);
    gd_key_init(KEY1, KEY_MODE_EXTI);
}

uint32_t Key0_StartTick = 0; 	//记录上升沿中断触发时的Tick
uint32_t Key0_StopTick = 0;	//记录下降沿中断触发时的Tick
void EXTI1_IRQHandler(void)
{
    if(RESET != exti_interrupt_flag_get(EXTI_1)) {
        if(RESET == gpio_input_bit_get(KEY0_GPIO_PORT, KEY0_PIN)){ Key0_StartTick = getTick(); } // 记录上升沿时刻的Tick值
        else if(SET == gpio_input_bit_get(KEY0_GPIO_PORT, KEY0_PIN)){ // 按键释放后，才算一次完整的按键输入
            Key0_StopTick = getTick();
            if(Key0_StopTick > (Key0_StartTick+50)){
                Key0_isPressed = true;
            }
        }
        exti_interrupt_flag_clear(EXTI_1);
    }
}

uint32_t Key1_StartTick = 0; 	//记录上升沿中断触发时的Tick
uint32_t Key1_StopTick = 0;	//记录下降沿中断触发时的Tick
void EXTI2_IRQHandler(void)
{
    if(RESET != exti_interrupt_flag_get(EXTI_2)) {
        if(RESET == gpio_input_bit_get(KEY1_GPIO_PORT, KEY1_PIN)){ Key1_StartTick = getTick(); } // 记录上升沿时刻的Tick值
        else if(SET == gpio_input_bit_get(KEY1_GPIO_PORT, KEY1_PIN)){ // 按键释放后，才算一次完整的按键输入
            Key1_StopTick = getTick();
            if(Key1_StopTick > (Key1_StartTick+50)){
                Key1_isPressed = true;
            }
        }
        exti_interrupt_flag_clear(EXTI_2);
    }
}
