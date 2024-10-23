#pragma once

#include "gd32f30x.h"
#include "gd32_hal.h"

#define LCD_CS_Pin          GPIO_PIN_3
#define LCD_CS_GPIO_Port    GPIOA
#define LCD_SCK_Pin         GPIO_PIN_5
#define LCD_SCK_GPIO_Port   GPIOA
#define LCD_SDA_Pin         GPIO_PIN_7
#define LCD_SDA_GPIO_Port   GPIOA

#define LCD_RST_Pin         GPIO_PIN_1
#define LCD_RST_GPIO_Port   GPIOA
#define LCD_BLK_Pin         GPIO_PIN_2
#define LCD_BLK_GPIO_Port   GPIOA
#define LCD_DC_Pin          GPIO_PIN_4
#define LCD_DC_GPIO_Port    GPIOA

void evse_ui_init(void);
GD_StatusTypeDef evse_lcd_spi_transmit(uint8_t *pData, uint16_t Size);
