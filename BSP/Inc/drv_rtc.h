#pragma once

#include "gd32f30x.h"

#define UTC_OFFSET (28800U)

void rtc_configuration(void);
void rtc_time_set(uint8_t hour, uint8_t minute, uint8_t second);
void rtc_date_set(uint8_t year, uint8_t month, uint8_t mday);
void rtc_set_alarm(uint8_t hour, uint8_t minute, uint8_t second);
void rtc_set_delay_alarm(uint8_t hour, uint8_t minute, uint8_t second);
