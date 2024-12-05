#pragma once

#include "gd32f30x.h"

#define FLASH_SIZE_128K         ((uint32_t)(2048U))
#define FLASH_PAGE_SIZE         ((uint32_t)(0x800U))
#define FLASH_EMPTY_DATA        ((uint32_t)0xFFFFFFFFU)

ErrStatus drv_flash_erase_page(uint32_t start_address, uint32_t size);
ErrStatus drv_flash_write_word(uint32_t address, uint32_t length, void* data_32);
