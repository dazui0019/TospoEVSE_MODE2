#include "gd32f30x.h"
#include "gd32f303x_start.h"
#include "delay.h"
#include "basic_os.h"
#include "main.h"
#include "string.h"
#include "printf_port.h"
#include "evse_cp.h"
#include "evse_comm.h"
#include "EventRecorder.h"
#include "evse_relay.h"
#include "evse_ntc.h"

#define LOG_TAG "evse.main"
#include "elog.h"

extern cp_t cp;

/* Stack for BasicOS */
__attribute__((used)) uint8_t stack[10240];

/* print out the clock frequency of system, AHB, APB1 and APB2 */
void print_clock(void)
{
    printf("CK_SYS is %d Hz\r\n", rcu_clock_freq_get(CK_SYS));
    printf("CK_AHB is %d Hz\r\n", rcu_clock_freq_get(CK_AHB));
    printf("CK_APB1 is %d Hz\r\n", rcu_clock_freq_get(CK_APB1));
    printf("CK_APB2 is %d Hz\r\n", rcu_clock_freq_get(CK_APB2));
}

int main(){
    uint16_t ob_ntc, pl_ntc;
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);
    
    EventRecorderInitialize(EventRecordAll, 1U);
    EventRecorderStart();

    evse_relay_init();
    evse_relay_ctrl(open);
    
    /* 初始化串口(Debug) */
    gd_usart_tx_init(COM0, 115200);
    retarget_printf(COM0);
    elog_init();
    elog_start();

    print_clock();
    log_d("Test.");

    /* 函数测试 */

    /* 启动BasicOS */
    systick_config();
    basic_os_init(stack, sizeof(stack));
    basic_os_run();

    return 0;
}

static void task_entry_blink(void *parameter)
{
    gd_led_init(LED0);
    for(;;){
        gd_led_toggle(LED0);
        bos_delay_ms(100);
    }
}
bos_task_export(blink, task_entry_blink, BOS_MAX_PRIORITY, NULL);
