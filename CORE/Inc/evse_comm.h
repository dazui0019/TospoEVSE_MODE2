#pragma once

#include "drv_uart.h"

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

typedef enum
{
    UI_STATE_REBOOT = 0,
    UI_STATE_IDLE,
    UI_STATE_WAIT_PLUGIN,
    UI_STATE_9V,
    UI_STATE_9V_PWM,
    UI_STATE_CHARGING,
    UI_STATE_DONE,
    UI_STATE_STOP,
    UI_STATE_ERROR
} evse_ui_state_t;

void evse_comm_init(void);
uint8_t evse_comm_ui_update(uint8_t cmd, uint8_t arg_int, void *arg_ptr);
