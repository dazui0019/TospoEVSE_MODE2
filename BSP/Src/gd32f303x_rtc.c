#include "gd32f303x_rtc.h"
#include "datetime.h"

//#define LOG_TAG    "rtc"
//#include "elog.h"

#define RTC_CLOCK_SOURCE_LXTAL

/**
 * @brief  configure the RTC peripheral(主要是配置时钟)
*/
void rtc_configuration(void)
{
    uint32_t RTCSRC_FLAG = 0; 
    /* get RTC clock entry selection */
    RTCSRC_FLAG = GET_BITS(RCU_BDCTL, 8, 9);

    if ((0xA5A5 != bkp_read_data(BKP_DATA_0)) || (0x00 == RTCSRC_FLAG)){
        /* enable PMU and BKPI clocks */
        rcu_periph_clock_enable(RCU_BKPI);
        rcu_periph_clock_enable(RCU_PMU);
        /* allow access to BKP domain */
        pmu_backup_write_enable();

        /* reset backup domain */
        bkp_deinit();

        /* enable RTC Clock */
        rcu_periph_clock_enable(RCU_RTC);

        /* wait until last write operation on RTC registers has finished */
        rtc_lwoff_wait();
        
        rtc_flag_clear(RTC_FLAG_ALARM|RTC_FLAG_SECOND|RTC_FLAG_OVERFLOW|RTC_FLAG_RSYN);
        
        /* wait until last write operation on RTC registers has finished */
        rtc_lwoff_wait();

        /* 修改RTC时钟源和预分频系数 */
        #if defined(RTC_CLOCK_SOURCE_LXTAL) // 外部低速晶振
            /* enable LXTAL */
            rcu_osci_on(RCU_LXTAL);
            /* wait till LXTAL is ready */
            rcu_osci_stab_wait(RCU_LXTAL);
            /* select RCU_LXTAL as RTC clock source */
            rcu_rtc_clock_config(RCU_RTCSRC_LXTAL);
            /* wait for RTC registers synchronization */
            rtc_register_sync_wait();
            /* wait until last write operation on RTC registers has finished */
            rtc_lwoff_wait();
            /* set RTC prescaler: set RTC period to 1s */
            rtc_prescaler_set(32767);   // 外部晶振 32.768 kHz
        #elif defined (RTC_CLOCK_SOURCE_LXTAL)
            // todo
        #else   // 使用内部低速RC振荡器
            #error RTC clock source should be defined.
        #endif

        /* wait until last write operation on RTC registers has finished */
        rtc_lwoff_wait();

        bkp_write_data(BKP_DATA_0, 0xA5A5);
    }else{
        /* check if the power on reset flag is set */
        if (rcu_flag_get(RCU_FLAG_PORRST) != RESET){
//            log_i("Power On Reset occurred....");
        }else if (rcu_flag_get(RCU_FLAG_SWRST) != RESET){
            /* check if the pin reset flag is set */
//            log_i("External Reset occurred....");
        }
//        log_i("No need to configure RTC....");
    }
    nvic_irq_enable(RTC_IRQn,5,0);
    rtc_lwoff_wait();
    pmu_backup_write_disable();
}

/**
 * @note UTC时间
*/
void rtc_time_set(uint8_t hour, uint8_t minute, uint8_t second)
{
    DateTime datetime;
    uint32_t current_timestamp;
    /* allow access to BKP domain */
    pmu_backup_write_enable();
    current_timestamp = rtc_counter_get();
    datetime_timestamp_to_datetime(current_timestamp, &datetime);

    /* set RTC time */
    datetime.second = second;
    datetime.minute = minute;
    datetime.hour   = hour;

    current_timestamp = datetime_datetime_to_timestamp(&datetime);

    rtc_counter_set(current_timestamp);
    rtc_lwoff_wait();
    pmu_backup_write_disable();
}

/**
 * @param day 一个月中的第几天
 * @note UTC时间
*/
void rtc_date_set(uint8_t year, uint8_t month, uint8_t day)
{
    DateTime datetime;
    uint32_t current_timestamp;
    /* allow access to BKP domain */
    pmu_backup_write_enable();
    current_timestamp = rtc_counter_get();
    datetime_timestamp_to_datetime(current_timestamp, &datetime);

    datetime.day    = day;
    datetime.month  = month;
    datetime.year   = (year+2000);

    current_timestamp = datetime_datetime_to_timestamp(&datetime);

    rtc_counter_set(current_timestamp);
    rtc_lwoff_wait();
    pmu_backup_write_disable();
}

/**
 * @brief  set the alarm time.传入的是当地时间，例如UTC+8
 * @param  hour: 时钟
 * @param  minute: 分钟
 * @param  second: 秒钟
 * @note   不同时区的时间戳是一样的。
 * @retval none
*/
void rtc_set_alarm(uint8_t hour, uint8_t minute, uint8_t second)
{
    DateTime datetime;
    uint32_t current_timestamp, alarm_timestamp;
    /* allow access to BKP domain */
    pmu_backup_write_enable();
    current_timestamp = rtc_counter_get();  // 获取当前的UTC时间戳
    // log_d("Current timestamp: %u", current_timestamp);
    alarm_timestamp = current_timestamp + UTC_OFFSET; // 由于在对比小时数时，是根据本地的时间来比较，所以直接将时间戳转换为UTC+8的数值(用UTC+8时间转换出来的时间戳)
    datetime_timestamp_to_datetime(alarm_timestamp, &datetime);     // 加上偏移后，算出格式化的本地时间
    // log_d("Current time: %0.2d:%0.2d:%0.2d", datetime.hour, datetime.minute, datetime.second);
    datetime.hour = (hour == 24) ? 0 : hour;
    datetime.minute = minute;
    datetime.second = second;
    alarm_timestamp = datetime_datetime_to_timestamp(&datetime);    // 然后用本地时间算出UTC时间戳(会比正常的时间戳多8小时)
    alarm_timestamp -= UTC_OFFSET;  // 转换回UTC时间戳
    if(alarm_timestamp < current_timestamp)
        alarm_timestamp += 86400;
//    log_d("Alarm timestamp: %u", (alarm_timestamp));

    rtc_lwoff_wait();
    rtc_alarm_config(alarm_timestamp);
    rtc_lwoff_wait();
    rtc_interrupt_enable(RTC_INT_ALARM);
    rtc_lwoff_wait();
    pmu_backup_write_disable();
}

/**
 * @brief  以延时的方式设置闹钟
*/
void rtc_set_delay_alarm(uint8_t hour, uint8_t minute, uint8_t second)
{
    DateTime datetime;
    uint32_t timestamp;
    /* allow access to BKP domain */
    pmu_backup_write_enable();

    timestamp = rtc_counter_get();

    timestamp += hour*3600;
    timestamp += minute*60;
    timestamp += second;

//    log_d("Alarm timestamp: %d", timestamp);
    rtc_lwoff_wait();
    rtc_alarm_config(timestamp); // UTC+8转换成UTC时间
    rtc_lwoff_wait();
    rtc_interrupt_enable(RTC_INT_ALARM);
    rtc_lwoff_wait();
    pmu_backup_write_disable();
}
