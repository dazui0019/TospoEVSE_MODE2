#pragma once

#include "gd32f30x.h"

/* 功能参数 */
// #define RFID_ENABLE             // RFID开启    
#define S1_CK_ENABLE            // S1检测开启
// #define CUR_ERR_CAN_BE_CLEAR    // 过流警告可以被清除
// #define RCD_CK_ENABLE           // RCD检测开启

#define EVSE_MAX_DELAY  (300)   // 最大延时上电时间(单位: 分钟)
#define EVSE_DELAY_SETP  (30)   // 延时上电时间步进值(单位: 分钟)

// 最大电流预设值
#define MAX_CUR_TABLE_VAL {6, 10, 13, 16, 20, 24, 32}
