#pragma once

#include "gd32f30x.h"

/**
 * @brief   获取正弦波正半波电压的平均值
 */
// uint16_t get_sin_val(__IO uint16_t pBuff[][3], uint16_t length, uint16_t index);
/**
 * @brief   初始化Vrefint采样
 */
void adc_verf_config(void);
/**
 * @brief   初始化AC检测
 */
void evse_ac_init(void);