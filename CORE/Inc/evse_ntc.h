#pragma once

#include "gd32f30x.h"

/**
 * @brief   初始化NTC
 */
void evse_ntc_init(void);
/**
 * @brief   获取板载NTC和电源插头NTC的原始值
 */
void evse_ntc_get_raw1(uint16_t *ob_raw, uint16_t *pl_raw);
void evse_ntc_get_raw2(uint16_t *ob_raw, uint16_t *pl_raw);
