#pragma once

#include "gd32f30x.h"
#include "gd32_hal.h"

/* UI 更新命令 */
#define UI_CMD_UPDATE_ALL           0U    // 更新所有数据
#define UI_CMD_UPDATE_VOLTAGE       1U    // 更新电压
#define UI_CMD_UPDATE_CURRENT       2U    // 更新电流
#define UI_CMD_UPDATE_POWER         3U    // 更新功率
#define UI_CMD_UPDATE_KWH           4U    // 更新电量
#define UI_CMD_SET_ERR              5U    // 更新错误信息
#define UI_CMD_RESET_ERR            6U    // 更新错误信息
#define UI_CMD_UPDATE_CNN_STATE     7U    // 更新枪线和汽车的连接状态
#define UI_CMD_UPDATE_DELAY         8U    // 更新延迟上电时间
#define UI_CMD_UPDATE_CHG_STATE     9U    // 更新充电状态
#define UI_CMD_UPDATE_STATE         10U   // 更新充电桩状态

uint8_t evse_ui_update(uint8_t cmd, uint16_t arg_int, void *arg_ptr);
