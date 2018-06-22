/*
 * This file is part of the TREZOR project, https://trezor.io/
 *
 * Copyright (C) 2014 Pavol Rusnak <stick@satoshilabs.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __OLED_H__
#define __OLED_H__

#include <stdint.h>
#include <stdbool.h>

#include "bitmaps.h"
#include "fonts.h"

#define OLED_WIDTH   96
#define OLED_HEIGHT  64

// OLED Color definitions
#define	BLACK           0x0000
#define	BLUE            0x001F
#define	RED             0xF800
#define	GREEN           0x07E0
#define CYAN            0x07FF
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0
#define WHITE           0xFFFF

// OLED palette colors.
#define OLED_BLACK 	(0x00)
#define OLED_YELLOW	(0x01)
#define	OLED_RED	(0x02)
#define OLED_BLUE	(0x03)
#define OLED_GREEN	(0x04)
#define OLED_WHITE 	(0x0F)

// To fit in 4KB of SRAM, use 4 bits per pixel, to
// map to up to 16 colors defined above. So, 2px per byte.
#define OLED_BUF_SIZE ((96 * 64) / 2)

void oledInit(void);
void oledClear(void);
void oledRefresh(void);

void oledSetDebugLink(bool set);
void oledInvertDebugLink(void);

void oledSetBuffer(uint8_t *buf);
const uint8_t *oledGetBuffer(void);

void oledDrawPixel(int x, int y,uint8_t color);
void oledClearPixel(int x, int y);
void oledInvertPixel(int x, int y);
void oledDrawChar(int x, int y, char c, int zoom, uint8_t color);
int  oledStringWidth(const char *text, int font);
void oledDrawString(int x, int y, const char* text, int font, uint8_t color);
void oledDrawStringCenter(int y, const char* text, int font);
void oledDrawStringRight(int x, int y, const char* text, int font);
void oledDrawBitmap(int x, int y, const BITMAP *bmp,uint8_t color);
void oledInvert(int x1, int y1, int x2, int y2);
void oledBox(int x1, int y1, int x2, int y2, bool set, uint8_t color);
void oledHLine(int y);
void oledFrame(int x1, int y1, int x2, int y2);
void oledSwipeLeft(void);
void oledSwipeRight(void);

#endif
