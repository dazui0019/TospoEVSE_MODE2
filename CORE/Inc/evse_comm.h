#pragma once

#include "drv_uart.h"

/* 更新显示的命令 */
#define FUNC_CODE_UPDATE_ALL        0x1E    // 更新所有数据
#define FUNC_CODE_UPDATE_CHG        0x28    // 更新充电状态
#define FUNC_CODE_UPDATE_ERR        0x29    // 更新故障状态
#define FUNC_CODE_UPDATE_KWH        0x2A    // 更新总电量
#define FUNC_CODE_UPDATE_DELAY      0x2B    // 更新延时时间
#define FUNC_CODE_UPDATE_CURRENT    0x2C    // 更新限制电流

/* 按键控制命令 */
#define FUNC_CODE_KEY_PRESS         0x3A    // 按键功能

#define FRAME_LEN_MAX               16      // 最大帧长度

void evse_comm_init(void);
uint8_t evse_comm_ui_update(uint8_t cmd, uint8_t arg_int, void *arg_ptr);
uint8_t evse_comm_check_sum(uint8_t pack[], uint16_t pack_len);
