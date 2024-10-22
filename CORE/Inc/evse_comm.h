#pragma once

#include "drv_uart.h"

typedef struct
{
    uint8_t state :1;
    uint8_t current :1;
    uint8_t delay :1;
    uint8_t voltage :1;
    uint8_t error :1;
} ui_update_flag_t;


#define UI_CMD_UPDATE_ALL       (uint32_t)0    // 更新所有数据
#define UI_CMD_UPDATE_VOLTAGE   (uint32_t)1    // 更新电压
#define UI_CMD_UPDATE_CURRENT   (uint32_t)2    // 更新电流
#define UI_CMD_UPDATE_POWER     (uint32_t)3    // 更新功率
#define UI_CMD_UPDATE_KWH       (uint32_t)4    // 更新电量
#define UI_CMD_UPDATE_ERR       (uint32_t)5    // 更新错误信息
#define UI_CMD_UPDATE_CNN_STATE (uint32_t)6    // 更新枪线和汽车的连接状态
#define UI_CMD_UPDATE_DELAY     (uint32_t)7    // 更新延迟上电时间
#define UI_CMD_UPDATE_CHG_STATE (uint32_t)8    // 更新充电状态

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
