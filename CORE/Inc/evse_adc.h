#pragma once

#include "gd32f30x.h"

/**
 * @brief   获取正弦波正半波电压的平均值
 */
uint16_t get_sin_vol(__IO uint16_t pBuff[][2], uint16_t length, uint16_t index);
