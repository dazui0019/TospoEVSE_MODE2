#pragma once

#include "gd32f30x.h"

void evse_ntc_config(void);
void evse_ntc_get_raw1(uint16_t *ob_raw, uint16_t *pl_raw);
void evse_ntc_get_raw2(uint16_t *ob_raw, uint16_t *pl_raw);
