#pragma once

#include "gd32f30x.h"
#include "gd32_hal.h"

#define LCD_SPI  SPI2

#define LCD_CS_Pin          GPIO_PIN_15
#define LCD_CS_GPIO_Port    GPIOA
#define LCD_SCK_Pin         GPIO_PIN_10
#define LCD_SCK_GPIO_Port   GPIOC
#define LCD_SDA_Pin         GPIO_PIN_12
#define LCD_SDA_GPIO_Port   GPIOC

#define LCD_RST_Pin         GPIO_PIN_2
#define LCD_RST_GPIO_Port   GPIOD
#define LCD_BLK_Pin         GPIO_PIN_11
#define LCD_BLK_GPIO_Port   GPIOC
#define LCD_DC_Pin          GPIO_PIN_3
#define LCD_DC_GPIO_Port    GPIOB

void lcd_gpio_config(void);
void lcd_spi_config(void);
GD_StatusTypeDef lcd_spi_transmit(uint8_t *pData, uint16_t Size);
GD_StatusTypeDef lcd_spi_transmit_dma(uint8_t *pData, uint16_t Size);
