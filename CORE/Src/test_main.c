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
#include "evse_beep.h"
#include "evse_cfg.h"

#define LOG_TAG "evse.main"
#include "elog.h"

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

/**
 * @brief   定时器初始化
 * @param   f 定时器更新频率, 单位Hz(1 - 1000000)
 * @note    TIMER2CLK(TIMER2_CK/PSC)固定为1000 000Hz(1MHz), 通过这个算出PSC寄存器的数值
*/
void evse_rcd_timer_init(uint16_t f)
{
    timer_parameter_struct timer_initpara;      // 定时器基本参数

    rcu_periph_clock_enable(RCU_TIMER3);

    timer_deinit(TIMER3);
    /* TIMER configuration */
    timer_initpara.prescaler         = ((timer_source_clock_get(TIMER3)/1000000U)-1); // TIMER3CLK(TIMER3_CK/PSC) is 100KHz
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_DOWN;
    timer_initpara.period            = (1000000U/f)-1;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(TIMER3,&timer_initpara);

    /* 开启定时器更新事件 */
    timer_update_event_enable(TIMER3);                                  // 配置TIMERx_CTL0的UPDIS
    timer_single_pulse_mode_config(TIMER3, TIMER_SP_MODE_REPETITIVE);   // 配置TIMERx_CTL0的SPM(配置为连续模式)
    timer_update_source_config(TIMER3, TIMER_UPDATE_SRC_REGULAR);       // 配置TIMERx_CTL0的UPS

    /* auto-reload preload enable */
    timer_auto_reload_shadow_enable(TIMER3);
    /* auto-reload preload enable */
    timer_enable(TIMER3);

    timer_interrupt_flag_clear(TIMER3, TIMER_INT_FLAG_UP);
    timer_interrupt_enable(TIMER3, TIMER_INT_UP);
    nvic_irq_enable(TIMER3_IRQn, 0U, 0U);
}

void TIMER3_IRQHandler(void)
{
    timer_disable(TIMER3);
    timer_counter_value_config(TIMER3, 999);
    timer_interrupt_flag_clear(TIMER3, TIMER_INT_FLAG_UP);
}

int main(){
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);
    
    EventRecorderInitialize(EventRecordAll, 1U);
    EventRecorderStart();
    
    rtc_reconfiguration();
    
    /* 初始化串口(Debug) */
    gd_usart_tx_init(COM0, 115200);
    retarget_printf(COM0);
    elog_init();
    elog_start();
    elog_set_filter_lvl(ELOG_LVL_ERROR);

    // print_clock();
    // log_d("Test.");

    delay_init();   // 初始化延时函数
    evse_rcd_timer_init(1000);

    /* 函数测试 */

    /* 启动BasicOS */
    systick_config();
    basic_os_init(stack, sizeof(stack));
    basic_os_run();

    return 0;
}
