#pragma once

#include "gd32f30x.h"

/* 功能参数 */
// #define RFID_ENABLE
#define S1_CK_ENABLE

#define EVSE_MAX_DELAY  (300)   // 最大延时上电时间(单位: 分钟)
#define EVSE_DELAY_SETP  (30)   // 延时上电时间步进值(单位: 分钟)

// 最大电流预设值
#define MAX_CUR_TABLE_VAL {6, 10, 13, 16, 20, 24, 32}
