/**
 * @file GC9A01.h
 *
 **/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "lcd_drv_conf.h"
#include "stdbool.h"
/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/* COMMON */

int GC9A01_init(void);
void GC9A01_setRotation(uint8_t m);
void GC9A01_fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void GC9A01_fillScreen(uint16_t color);
uint16_t GC9A01_Color565(uint8_t r, uint8_t g, uint8_t b);
void GC9A01_invertDisplay(bool i);
void GC9A01_drawPixel(int16_t x, int16_t y, uint16_t color);
void GC9A01_drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
void GC9A01_drawFastVLine(int16_t x, int16_t y, int16_t w, uint16_t color);

/* MINE */

void GC9A01_drawCheckMark(int16_t x, int16_t y, uint16_t color, uint8_t thickness);
void GC9A01_drawCross(int16_t x, int16_t y, uint16_t color, uint8_t thickness);
void GC9A01_drawThickLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color, uint8_t thickness);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif
