#pragma once

#include "gd32f30x.h"

/**
 * @brief   初始化Vrefint采样
 */
void adc_verf_config(void);
/**
 * @brief   初始化AC检测
 */
void evse_ac_init(void);

/**
 * @brief   粘连检测
 * @return  ErrStatus
 *          @retval  ERROR   粘连
 *          @retval  SUCCESS 未粘连
 */
ErrStatus evse_ac_adh_ck(void);
