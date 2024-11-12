#pragma once

#include "stdint.h"
#include "gd32f30x.h"

#define FMC_PAGE_SIZE               ((uint16_t)0x800U)

#define CFG_BASE_ADDR               ((uint32_t)0x08030000U)                             // IAP区起始地址

/* CUR: 2KB(0x08030000 - 0x08003FFFU) */
#define CFG_CUR_SIZE                FMC_PAGE_SIZE                                       // IAP区大小为2KB(1页)
#define CFG_CUR_START_ADDR          CFG_BASE_ADDR                                       // 0x08030000
#define CFG_CUR_END_ADDR            ((uint32_t)(CFG_CUR_START_ADDR+(CFG_CUR_SIZE-1U)))  // 0x08003FFFU

void evse_cfg_write_cur(uint32_t cur);
void evse_cfg_erase(void);
