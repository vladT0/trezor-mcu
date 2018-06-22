/*
 * This file is part of the TREZOR project, https://trezor.io/
 *
 * Copyright (C) 2014 Pavol Rusnak <stick@satoshilabs.com>
 * Copyright (C) 2018 vladT0
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

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>

#include <string.h>

#include "oled.h"
#include "util.h"

#define SPI_BASE				SPI1
#define OLED_DC_PORT			GPIOB
#define OLED_DC_PIN				GPIO0	// PB0 | Data/Command
#define OLED_CS_PORT			GPIOA
#define OLED_CS_PIN				GPIO4	// PA4 | SPI Select
#define OLED_RST_PORT			GPIOB
#define OLED_RST_PIN			GPIO1	// PB1 | Reset display

/*
 * This library was modified for color display SSD1331 OLED_WIDTH x OLED_HEIGHT (96x64)
 * The contents of this display are buffered in oledbuffer.  This is
 * an array of OLED_WIDTH * OLED_HEIGHT/2 bytes. Palette has 16 colors.
 * The pixel (0,0) is the top left corner of the display.
 */

static bool is_debug_link = 0;
static uint8_t oled_buffer[OLED_BUF_SIZE]; //color buffer
static uint8_t oled_pixels[4];

// Color palette.
static uint16_t oled_colors[16] = {
  0x0000, 0xFFE0, 0xF800, 0x001F,
  0xF81F, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0xFFFF
};

/*
 * macros to convert coordinate to half byte position
 */

#define OLED_OFFSET(x, y) (x/2) + (y*OLED_WIDTH)/2

/*
 * Draws a color pixel at x, y
 */

void oledDrawPixel(int x, int y, uint8_t color)
{
	uint8_t *cell;

	if ((x < 0) || (y < 0) || (x >= OLED_WIDTH) || (y >= OLED_HEIGHT)) {
		return;
	}

	cell = &oled_buffer[OLED_OFFSET(x, y)];
	*cell = (*cell & (0x0F<<4*((x%2)))) | (color << 4*((x%2)^0x1));
}

/*
 * Clears pixel ( draw black ) at x, y
 */
void oledClearPixel(int x, int y)
{
	uint8_t *cell;

	if ((x < 0) || (y < 0) || (x >= OLED_WIDTH) || (y >= OLED_HEIGHT)) {
		return;
	}

	cell = &oled_buffer[OLED_OFFSET(x, y)];
	*cell = *cell & (0x0F<<4*((x%2)));
}

/*
 * Inverts pixel at x, y
 */
void oledInvertPixel(int x, int y)
{
	uint8_t *cell;

	if ((x < 0) || (y < 0) || (x >= OLED_WIDTH) || (y >= OLED_HEIGHT)) {
		return;
	}

	cell = &oled_buffer[OLED_OFFSET(x, y)];
	*cell = *cell ^ (0x0F<<4*((x%2)));
}

#if !EMULATOR
/*
 * Send a block of data via the SPI bus.
 */
static inline void SPISend(uint32_t base, const uint8_t *data, int len)
{
	delay(1);
	for (int i = 0; i < len; i++) {
		spi_send(base, data[i]);
	}
	while (!(SPI_SR(base) & SPI_SR_TXE));
	while ((SPI_SR(base) & SPI_SR_BSY));
}

/*
 * Initialize the display.
 */
void oledInit()
{
        static const uint8_t s[46] = {

	// 'Unlock Display.' - 0xFD/0x16 can 'Lock' it,
	// in which case all other commands are ignored.
		0xFD,
		0x12,
  	// (Turn the display off.)
		0xAE,
  	// 'Set Column Address' - default is 0-95, which is
  	// also what we want.
		0x15,
		0x00,
		0x5F,
  	// 'Set Row Address' - default is 0-63, which is good.
		0x75,
		0x00,
		0x3F,
  	// 'Set Color A Contrast' - default is 128.
		0x81,
		0x80,
  	// 'Set Color B Contrast' - default is 128, use 96.
		0x82,
		0x60,
  	// 'Set Color C Contrast' - default is 128.
		0x83,
		0x80,
  	// 'Set Master Current Control' - default is 15, but
  	// use 8 for ~half. (~= 'Set Brightness')
		0x87,
		0x08,
  	// 'Set Precharge A' - default is 'Color A Contrast'.
		0x8A,
		0x80,
  	// 'Set Precharge B' - default is 'Color B Contrast'.
		0x8B,
		0x60,
  	// 'Set Precharge C' - default is 'Color C Contrast'.
		0x8C,
		0x80,
  	// 'Remap Display Settings' - default is 0x40.
  	// Use 0x60 to avoid drawing lines in odd-even order.
		0xA0,
		0x60,
  	// 'Set Display Start Row' - default is 0.
		0xA1,
		0x00,
  	// 'Set Vertical Offset' - default is 0.
		0xA2,
		0x00,
  	// 'Set Display Mode' - default is 'A4'. 'A7' = invert.
  	// (The actual command byte sets the mode; no 'arg')
		0xA4,
  	// 'Set Multiplex Ratio.' I think this is how many
  	// rows of pixels are actually enabled; default is 63.
		0xA8,
		0x3F,
  	// (I am going to ignore the 0xAB 'Dim Mode Settings'
  	// command - it looks like it only matters if we use
  	// the 0xAC 'Dim Display' command; we will use 0xAF.)
  	// 'Set Voltage Supply Configuration'. The SSD1331 has
  	// no onboard charge pump, so we must use external
  	// voltage. (0x8E)
		0xAB,
		0x8E,
  	// 'Set Power Save Mode'. Default enabled; disable it.
  	// ('on' is 0x1A, 'off' is 0x0B)
		0xB0,
		0x0B,
  	// 'Adjust Precharge Phases.' Bits [7:4] set the
  	// precharge stage 2 period, bits [3:0] set phase 1.
  	// Default is 0x74.
		0xB1,
		0x74,
  	// 'Set Clock Divider Frequency'. Bits [7:4] set the
  	// oscillator frequency, bits [3:0]+1 set the
  	// clock division ratio. Default is 0xD0.
		0xB3,
		0xD0,
  	// (I am going to ignore the 'Set Gray scale Table'
  	// command - it has a bunch of gamma curve settings.)
  	// So, the 'Reset to Default Gray scale Table'
  	// command does make sense to call.
		0xB9,
  	// 'Set Precharge Level'. Default is 0x3E.
		0xBB,
		0x3E,
  	// 'Set Logic 0 Threshold'. Default is 0x3E = 0.83*VCC.
		0xBE,
		0x3E,
  	// 'Display On'.
		0xAF
	};

	gpio_clear(OLED_DC_PORT, OLED_DC_PIN);		// set to CMD
	gpio_set(OLED_CS_PORT, OLED_CS_PIN);		// SPI deselect

	// Reset the LCD
	gpio_set(OLED_RST_PORT, OLED_RST_PIN);
	delay(40);
	gpio_clear(OLED_RST_PORT, OLED_RST_PIN);
	delay(400);
	gpio_set(OLED_RST_PORT, OLED_RST_PIN);

	// init
	gpio_clear(OLED_CS_PORT, OLED_CS_PIN);		// SPI select
	SPISend(SPI_BASE, s, 46);
	gpio_set(OLED_CS_PORT, OLED_CS_PIN);		// SPI deselect

	oledClear();
	oledRefresh();
}
#endif

/*
 * Clears the display buffer (sets all pixels to black)
 */
void oledClear()
{
	memset(oled_buffer, 0, sizeof(oled_buffer));
}

void oledInvertDebugLink()
{
	if (is_debug_link) {
		oledInvertPixel(OLED_WIDTH - 5, 0); oledInvertPixel(OLED_WIDTH - 4, 0); oledInvertPixel(OLED_WIDTH - 3, 0); oledInvertPixel(OLED_WIDTH - 2, 0); oledInvertPixel(OLED_WIDTH - 1, 0);
		oledInvertPixel(OLED_WIDTH - 4, 1); oledInvertPixel(OLED_WIDTH - 3, 1); oledInvertPixel(OLED_WIDTH - 2, 1); oledInvertPixel(OLED_WIDTH - 1, 1); 
		oledInvertPixel(OLED_WIDTH - 3, 2); oledInvertPixel(OLED_WIDTH - 2, 2); oledInvertPixel(OLED_WIDTH - 1, 2);
		oledInvertPixel(OLED_WIDTH - 2, 3); oledInvertPixel(OLED_WIDTH - 1, 3);
		oledInvertPixel(OLED_WIDTH - 1, 4);
	}
}

/*
 * Refresh the display. This copies the buffer to the display to show the
 * contents.  This must be called after every operation to the buffer to
 * make the change visible.  All other operations only change the buffer
 * not the content of the display.
 */
#if !EMULATOR
void oledRefresh()
{
	static const uint8_t s[10] =
	{
        // 'Set Column Address' - default is 0-95, which is
        // also what we want.
                0x15,
                0x00,
                0x5F,
        // 'Set Row Address' - default is 0-63, which is good.
                0x75,
                0x00,
                0x3F,
        // 'Set Display Start Row' - default is 0.
                0xA1,
                0x00,
	// 'Set Display Offset' - default is 0.
		0xA2,
		0x00
	};

 	uint16_t px_i = 0;
 	uint8_t px_col = 0;

	// draw triangle in upper right corner
	oledInvertDebugLink();

	gpio_clear(OLED_CS_PORT, OLED_CS_PIN);		// SPI select
	SPISend(SPI_BASE, s, 10);
	gpio_set(OLED_CS_PORT, OLED_CS_PIN);		// SPI deselect

	gpio_set(OLED_DC_PORT, OLED_DC_PIN);		// set to DATA
	gpio_clear(OLED_CS_PORT, OLED_CS_PIN);		// SPI select


  	for (px_i = 0; px_i < OLED_BUF_SIZE; ++px_i) {
      		px_col = oled_buffer[px_i] >> 4;
      		oled_pixels[0] = ((uint8_t*)&(oled_colors[px_col]))[1];
      		oled_pixels[1] = ((uint8_t*)&(oled_colors[px_col]))[0];
      		px_col = oled_buffer[px_i] & 0x0F;
      		oled_pixels[2] = ((uint8_t*)&(oled_colors[px_col]))[1];
      		oled_pixels[3] = ((uint8_t*)&(oled_colors[px_col]))[0];
  	        SPISend(SPI_BASE,oled_pixels,4);
	}

	gpio_set(OLED_CS_PORT, OLED_CS_PIN);		// SPI deselect
	gpio_clear(OLED_DC_PORT, OLED_DC_PIN);		// set to CMD

	// return it back
	oledInvertDebugLink();
}
#endif

const uint8_t *oledGetBuffer()
{
	return oled_buffer;
}

void oledSetDebugLink(bool set)
{
	is_debug_link = set;
	oledRefresh();
}

void oledSetBuffer(uint8_t *buf)
{
	memcpy(oled_buffer, buf, sizeof(oled_buffer));
}

void oledDrawChar(int x, int y, char c, int font, uint8_t color)
{
	if (x >= OLED_WIDTH || y >= OLED_HEIGHT || y <= -FONT_HEIGHT) {
		return;
	}

	int zoom = (font & FONT_DOUBLE ? 2 : 1);
	int char_width = fontCharWidth(font & 0x7f, c);
	const uint8_t *char_data = fontCharData(font & 0x7f, c);

	if (x <= -char_width * zoom) {
		return;
	}

	for (int xo = 0; xo < char_width; xo++) {
		for (int yo = 0; yo < FONT_HEIGHT; yo++) {
			if (char_data[xo] & (1 << (FONT_HEIGHT - 1 - yo))) {
				if (zoom <= 1) {
					oledDrawPixel(x + xo, y + yo, color);
				} else {
					oledBox(x + xo * zoom, y + yo * zoom, x + (xo + 1) * zoom - 1, y + (yo + 1) * zoom - 1, true, color);
				}
			}
		}
	}
}

char oledConvertChar(const char c) {
	uint8_t a = c;
	if (a < 0x80) return c;
	// UTF-8 handling: https://en.wikipedia.org/wiki/UTF-8#Description
	// bytes 11xxxxxx are first byte of UTF-8 characters
	// bytes 10xxxxxx are successive UTF-8 characters
	if (a >= 0xC0) return '_';
	return 0;
}

int oledStringWidth(const char *text, int font) {
	if (!text) return 0;
	int size = (font & FONT_DOUBLE ? 2 : 1);
	int l = 0;
	for (; *text; text++) {
		char c = oledConvertChar(*text);
		if (c) {
			l += size * (fontCharWidth(font & 0x7f, c) + 1);
		}
	}
	return l;
}

void oledDrawString(int x, int y, const char* text, int font, uint8_t color)
{
	if (!text) return;
	int l = 0;
	int size = (font & FONT_DOUBLE ? 2 : 1);
	for (; *text; text++) {
		char c = oledConvertChar(*text);
		if (c) {
			oledDrawChar(x + l, y, c, font,color);
			l += size * (fontCharWidth(font & 0x7f, c) + 1);
		}
	}
}

void oledDrawStringCenter(int y, const char* text, int font)
{
	int x = ( OLED_WIDTH - oledStringWidth(text, font) ) / 2;
	oledDrawString(x, y, text, font, OLED_WHITE);
}

void oledDrawStringRight(int x, int y, const char* text, int font)
{
	x -= oledStringWidth(text, font);
	oledDrawString(x, y, text, font, OLED_WHITE);
}

void oledDrawBitmap(int x, int y, const BITMAP *bmp, uint8_t color)
{
	for (int i = 0; i < bmp->width; i++) {
		for (int j = 0; j < bmp->height; j++) {
			if (bmp->data[(i / 8) + j * bmp->width / 8] & (1 << (7 - i % 8))) {
				oledDrawPixel(x + i, y + j, color);
			} else {
				oledClearPixel(x + i, y + j);
			}
		}
	}
}

/*
 * Inverts box between (x1,y1) and (x2,y2) inclusive.
 */
void oledInvert(int x1, int y1, int x2, int y2)
{
	x1 = MAX(x1, 0);
	y1 = MAX(y1, 0);
	x2 = MIN(x2, OLED_WIDTH - 1);
	y2 = MIN(y2, OLED_HEIGHT - 1);
	for (int x = x1; x <= x2; x++) {
		for (int y = y1; y <= y2; y++) {
			oledInvertPixel(x,y);
		}
	}
}

/*
 * Draw a filled rectangle.
 */
void oledBox(int x1, int y1, int x2, int y2, bool set, uint8_t color)
{
	x1 = MAX(x1, 0);
	y1 = MAX(y1, 0);
	x2 = MIN(x2, OLED_WIDTH - 1);
	y2 = MIN(y2, OLED_HEIGHT - 1);
	for (int x = x1; x <= x2; x++) {
		for (int y = y1; y <= y2; y++) {
			set ? oledDrawPixel(x, y, color) : oledClearPixel(x, y);
		}
	}
}

void oledHLine(int y) {
	if (y < 0 || y >= OLED_HEIGHT) {
		return;
	}
	for (int x = 0; x < OLED_WIDTH; x++) {
		oledDrawPixel(x, y, OLED_WHITE);
	}
}

/*
 * Draw a rectangle frame.
 */
void oledFrame(int x1, int y1, int x2, int y2)
{
	for (int x = x1; x <= x2; x++) {
		oledDrawPixel(x, y1, OLED_WHITE);
		oledDrawPixel(x, y2, OLED_WHITE);
	}
	for (int y = y1 + 1; y < y2; y++) {
		oledDrawPixel(x1, y, OLED_WHITE);
		oledDrawPixel(x2, y, OLED_WHITE);
	}
}

void ShiftLeftHalfByte(uint8_t *array, int len)
{
    for (int i = 0;  i < len;  i++)
    {
        if (i <= (len - 1))
        {
            array[i]  = array[i] << 4;
            if (i != (len - 1))
            	array[i] |= array[i + 1] >> 4;
        }
        else
        	array[i] = 0;
    }
}

void ShiftRightHalfByte(uint8_t *array, int len)
{
	uint8_t bits1,bits2;
	for(int i = len-1; i >= 0; --i)
	{
		bits1 = (array[i] & 0xF0) >> 4;
		if(i)
			bits2 = (array[i-1] & 0x0F) << 4;
		else
			bits2 = 0;
		array[i] = bits1 | bits2;
	}
}

/*
 * Animates the display, swiping the current contents out to the left.
 * This clears the display.
 */
void oledSwipeLeft(void)
{
	for (int i = 0; i < OLED_WIDTH; i++) {
		for (int y = 0; y < OLED_HEIGHT; y++) {
			uint8_t *in = &(oled_buffer[OLED_WIDTH/2*y]);
			ShiftLeftHalfByte(in,OLED_WIDTH/2);
		}
		oledRefresh();
		delay(20000);
	}
}

/*
 * Animates the display, swiping the current contents out to the right.
 * This clears the display.
 */
void oledSwipeRight(void)
{
	for (int i = 0; i < OLED_WIDTH; i++) {
		for (int y = 0; y < OLED_HEIGHT; y++) {
			uint8_t *in = &(oled_buffer[OLED_WIDTH/2*y]);
			ShiftRightHalfByte(in,OLED_WIDTH/2);
		}
		oledRefresh();
		delay(20000);
	}
}
