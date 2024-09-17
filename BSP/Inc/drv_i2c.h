#pragma once

#include "gd32f30x.h"
#include "gd32_hal.h"

/*!
    \brief      configure the I2C0 interfaces
    \param[in]  none
    \param[out] none
    \retval     none
*/
void i2c0_master_config(void);
GD_StatusTypeDef i2c0_send_dma(uint8_t slaver_addr, const uint8_t pData[], uint32_t Size);
