#include "gd32f30x.h"
#include "gd32f303x_start.h"
#include "drv_delay.h"
#include "basic_os.h"
#include "main.h"
#include "string.h"
#include "printf_port.h"
#include "evse_cp.h"
#include "evse_comm.h"
#include "EventRecorder.h"
#include "evse_relay.h"
#include "evse_ntc.h"
#include "evse_rcd.h"
#include "evse_ui.h"
#include "GC9A01.h"
#include "lcd_drv_conf.h"
#include "drv_rtc.h"

#define LOG_TAG "evse.main"
#include "elog.h"

uint8_t temp_array[48];
void a_ws2812b_write_one_frame(uint32_t rgb, uint8_t temp[48]);

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
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);
    
    EventRecorderInitialize(EventRecordAll, 1U);
    EventRecorderStart();
    

    rtc_reconfiguration();

    evse_relay_init();
    evse_relay_ctrl(open);
    
    /* 初始化串口(Debug) */
    gd_usart_tx_init(COM0, 115200);
    retarget_printf(COM0);
    elog_init();
    elog_start();

    // print_clock();
    log_d("Test.");

    delay_init();   // 初始化延时函数
    /* 需要纯延时的初始化函数 */
    // evse_rcd_init();

    // delay_deinit(); // 重置用于延时的定时器

    /* 函数测试 */
    // a_ws2812b_write_one_frame(0xFF0000, temp_array);

    /* 启动BasicOS */
    systick_config();
    basic_os_init(stack, sizeof(stack));
    basic_os_run();

    return 0;
}

static void task_entry_blink(void *parameter)
{
    gd_led_init(LED2);
    for(;;){
        gd_led_toggle(LED2);
        bos_delay_ms(100);
    }
}
bos_task_export(blink, task_entry_blink, BOS_MAX_PRIORITY, NULL);

void a_ws2812b_write_one_frame(uint32_t rgb, uint8_t temp[48])
{
    uint8_t r, g, b;
    uint8_t i, j;
    uint32_t c, point;
    const uint16_t one_code = 0xFFF8U;
    const uint16_t zero_code = 0xE000U;
    
    r = (uint8_t)((rgb >> 16) & 0xFF);                               /* set red */
    g = (uint8_t)((rgb >> 8) & 0xFF);                                /* set green */
    b = (uint8_t)((rgb >> 0) & 0xFF);                                /* set blue */
    c = ((uint32_t)(g) << 16) | ((uint32_t)(r) << 8) | b;            /* set color */
    EventStartA(7);
    memset(temp, 0, sizeof(uint8_t) * 30);                           /* clear the temp buffer */
    EventStopA(7);
    point = 0;                                                       /* clear point */
    for (i = 0; i < 24; i++)                                         /* set 24 bit */
    {
        if (((c >> (23 - i)) & 0x01) != 0)                           /* if bit 1 */
        {
            // temp[2*i] = 0xFF;
            // temp[2*i + 1] = 0xF8;
            *(uint16_t*)(temp + 2*i) = 0xF8FF;
            // for (j = 0; j < 16; j ++)                                /* 16 bit */
            // {
            //     if (((one_code >> (15 - j)) & 0x01) != 0)            /* if one code */
            //     {
            //         temp[point / 8] |= 1 << (7 - (point % 8));       /* set bit 1 */
            //     }
            //     else
            //     {
            //         temp[point / 8] |= 0 << (7 - (point % 8));       /* set bit 0 */
            //     }
            //     point = point + 1;                                   /* point++ */
            // }
        }
        else                                                         /* if bit 0 */
        {
            // temp[2*i] = 0xE0;
            // temp[2*i + 1] = 0x00;
            *(uint16_t*)(temp + 2*i) = 0x00E0;
            // for (j = 0; j < 16; j ++)                                /* 16 bit */
            // {
            //     if (((zero_code >> (15 - j)) & 0x01) != 0)           /* if zero code */
            //     {
            //         temp[point / 8] |= 1 << (7 - (point % 8));       /* set bit 1 */
            //     }
            //     else
            //     {
            //         temp[point / 8] |= 0 << (7 - (point % 8));       /* set bit 0 */
            //     }
            //     point = point + 1;                                   /* point++ */
            // }
        }
    }
}
