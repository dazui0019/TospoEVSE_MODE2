#pragma once

#include "main.h"
#include "driver_ws2812b.h"


/************************************************************************************************************
**************    Configurations
************************************************************************************************************/
#define WS28XX_ORDER_RGB                        0
#define WS28XX_ORDER_BGR                        1
#define WS28XX_ORDER_GRB                        2
#define WS28XX_ORDER_RBG                        3

/*---------- WS28XX_ORDER  -----------*/
#define WS28XX_ORDER      WS28XX_ORDER_RGB


/************************************************************************************************************
**************    Public Definitions
************************************************************************************************************/

#define COLOR_RGB565_BLACK         0x0000      /*   0,   0,   0 */
#define COLOR_RGB565_WHITE         0xFFFF      /* 255, 255, 255 */
#define COLOR_RGB565_RED           0xF800      /* 255,   0,   0 */
#define COLOR_RGB565_GREEN         0x07E0      /*   0, 255,   0 */
#define COLOR_RGB565_BLUE          0x001F      /*   0,   0, 255 */
#define COLOR_RGB565_CYAN          0x07FF      /*   0, 255, 255 */
#define COLOR_RGB565_MAGENTA       0xF81F      /* 255,   0, 255 */
#define COLOR_RGB565_YELLOW        0xFFE0      /* 255, 255,   0 */
#define COLOR_RGB565_ORANGE        0xFD20      /* 255, 165,   0 */
#define COLOR_RGB565_PINK          0xFC18      /* 255, 192, 203 */
#define COLOR_RGB565_PURPLE        0x780F      /* 128,   0, 128 */
#define COLOR_RGB565_TEAL          0x0438      /*   0, 128, 128 */
#define COLOR_RGB565_LIME          0x07E0      /*   0, 255,   0 */
#define COLOR_RGB565_AQUA          0x5D1C      /*   0, 255, 255 */
#define COLOR_RGB565_MAROON        0x7800      /* 128,   0,   0 */
#define COLOR_RGB565_NAVY          0x000F      /*   0,   0, 128 */
#define COLOR_RGB565_OLIVE         0x7BE0      /* 128, 128,   0 */
#define COLOR_RGB565_SILVER        0xC618      /* 192, 192, 192 */
#define COLOR_RGB565_GRAY          0x8410      /* 128, 128, 128 */
#define COLOR_RGB565_SKYBLUE       0x867D      /* 135, 206, 235 */
#define COLOR_RGB565_VIOLET        0x915C      /* 138, 43, 226 */
#define COLOR_RGB565_BROWN         0xA145      /* 165, 42, 42 */
#define COLOR_RGB565_GOLD          0xFEA0      /* 255, 215, 0 */
#define COLOR_RGB565_TAN           0xD5B1      /* 210, 180, 140 */
#define COLOR_RGB565_FORESTGREEN   0x2444      /* 34, 139, 34 */
#define COLOR_RGB565_SEAGREEN      0x2C4A      /* 46, 139, 87 */
#define COLOR_RGB565_CRIMSON       0xD8A7      /* 220, 20, 60 */
#define COLOR_RGB565_LAVENDER      0xE73B      /* 230, 230, 250 */
#define COLOR_RGB565_INDIGO        0x4810      /* 75, 0, 130 */
#define COLOR_RGB565_DARKORANGE    0xFC60      /* 255, 140, 0 */
#define COLOR_RGB565_CHOCOLATE     0xCB6D      /* 210, 105, 30 */
#define COLOR_RGB565_FIREBRICK     0xB104      /* 178, 34, 34 */
#define COLOR_RGB565_CORAL         0xFBEA      /* 255, 127, 80 */
#define COLOR_RGB565_TOMATO        0xFB08      /* 255, 99, 71 */
#define COLOR_RGB565_SLATEGRAY     0x7412      /* 112, 128, 144 */
#define COLOR_RGB565_DARKSLATEGRAY 0x2A69      /* 47, 79, 79 */
#define COLOR_RGB565_DIMGRAY       0x6B4D      /* 105, 105, 105 */

#define COLOR_RGB888_BLACK         0x000000    /*   0,   0,   0 */
#define COLOR_RGB888_WHITE         0xFFFFFF    /* 255, 255, 255 */
#define COLOR_RGB888_RED           0xFF0000    /* 255,   0,   0 */
#define COLOR_RGB888_GREEN         0x00FF00    /*   0, 255,   0 */
#define COLOR_RGB888_BLUE          0x0000FF    /*   0,   0, 255 */
#define COLOR_RGB888_CYAN          0x00FFFF    /*   0, 255, 255 */
#define COLOR_RGB888_MAGENTA       0xFF00FF    /* 255,   0, 255 */
#define COLOR_RGB888_YELLOW        0xFFFF00    /* 255, 255,   0 */
#define COLOR_RGB888_ORANGE        0xFFA500    /* 255, 165,   0 */
#define COLOR_RGB888_PINK          0xFFC0CB    /* 255, 192, 203 */
#define COLOR_RGB888_PURPLE        0x800080    /* 128,   0, 128 */
#define COLOR_RGB888_TEAL          0x008080    /*   0, 128, 128 */
#define COLOR_RGB888_LIME          0x00FF00    /*   0, 255,   0 */
#define COLOR_RGB888_AQUA          0x00FFFF    /*   0, 255, 255 */
#define COLOR_RGB888_MAROON        0x800000    /* 128,   0,   0 */
#define COLOR_RGB888_NAVY          0x000080    /*   0,   0, 128 */
#define COLOR_RGB888_OLIVE         0x808000    /* 128, 128,   0 */
#define COLOR_RGB888_SILVER        0xC0C0C0    /* 192, 192, 192 */
#define COLOR_RGB888_GRAY          0x808080    /* 128, 128, 128 */
#define COLOR_RGB888_SKYBLUE       0x87CEEB    /* 135, 206, 235 */
#define COLOR_RGB888_VIOLET        0x8A2BE2    /* 138, 43, 226 */
#define COLOR_RGB888_BROWN         0xA52A2A    /* 165, 42, 42 */
#define COLOR_RGB888_GOLD          0xFFD700    /* 255, 215, 0 */
#define COLOR_RGB888_TAN           0xD2B48C    /* 210, 180, 140 */
#define COLOR_RGB888_FORESTGREEN   0x228B22    /* 34, 139, 34 */
#define COLOR_RGB888_SEAGREEN      0x2E8B57    /* 46, 139, 87 */
#define COLOR_RGB888_CRIMSON       0xDC143C    /* 220, 20, 60 */
#define COLOR_RGB888_LAVENDER      0xE6E6FA    /* 230, 230, 250 */
#define COLOR_RGB888_INDIGO        0x4B0082    /* 75, 0, 130 */
#define COLOR_RGB888_DARKORANGE    0xFF8C00    /* 255, 140, 0 */
#define COLOR_RGB888_CHOCOLATE     0xD2691E    /* 210, 105, 30 */
#define COLOR_RGB888_FIREBRICK     0xB22222    /* 178, 34, 34 */
#define COLOR_RGB888_CORAL         0xFF7F50    /* 255, 127, 80 */
#define COLOR_RGB888_TOMATO        0xFF6347    /* 255, 99, 71 */
#define COLOR_RGB888_SLATEGRAY     0x708090    /* 112, 128, 144 */
#define COLOR_RGB888_DARKSLATEGRAY 0x2F4F4F    /* 47, 79, 79 */
#define COLOR_RGB888_DIMGRAY       0x696969    /* 105, 105, 105 */

typedef struct {
    uint32_t r : 8;
    uint32_t g : 8;
    uint32_t b : 8;
}color_t;

/**
 * SPI CLK = 10MHz
 */
uint8_t ws2812b_spi_init();

uint8_t ws2812b_spi_deinit();
uint8_t ws2812b_write_cmd(uint8_t *buf, uint16_t len);
