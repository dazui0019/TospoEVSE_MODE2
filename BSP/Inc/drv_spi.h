#pragma once

#include <stdint.h>
#include "gd32_hal.h"
#include "gd32f30x.h"

GD_StatusTypeDef GD_SPI_Transmit(uint32_t spi_periph, uint8_t *pData, uint16_t Size, uint32_t Timeout);
GD_StatusTypeDef SPI_Transmit(uint32_t spi_periph, uint8_t *pData, uint16_t Size);
