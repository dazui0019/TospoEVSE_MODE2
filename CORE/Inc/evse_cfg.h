#pragma once

#include <stdint.h>
#include <stddef.h>
#include "gd32f30x.h"
#include "drv_flash.h"


#define FMC_PAGE_SIZE               ((uint16_t)0x800U)

#define CFG_BASE_ADDR               ((uint32_t)0x08030000U)                             // CFG区起始地址

/* CUR: 2KB(0x08030000 - 0x08003FFFU) */
#define CFG_CUR_SIZE                FMC_PAGE_SIZE                                       // CFG区大小为2KB(1页)
#define CFG_CUR_START_ADDR          CFG_BASE_ADDR                                       // 0x08030000
#define CFG_CUR_END_ADDR            ((uint32_t)(CFG_CUR_START_ADDR+(CFG_CUR_SIZE-1U)))  // 0x08003FFFU

typedef struct __attribute__((packed, aligned(sizeof(uint8_t)))){
    uint32_t max_cur_idx;
} evse_cfg_t;

void evse_cfg_write(evse_cfg_t* cfg);
void evse_cfg_erase(void);
void evse_cfg_get_last(evse_cfg_t* cfg);
#define EVSE_CFG_WRITE(p_cfg)   drv_flash_write_word(CFG_CUR_START_ADDR, sizeof(evse_cfg_t)>>2, p_cfg)
