/*
   =================================================================================
Name        : PCD8544.c
Version     : 0.1

Copyright (C) 2010 Limor Fried, Adafruit Industries
CORTEX-M3 version by Le Dang Dung, 2011 LeeDangDung@gmail.com (tested on LPC1769)
Raspberry Pi version by Andre Wussow, 2012, desk@binerry.de

Description :
A simple PCD8544 LCD (Nokia3310/5110) driver. Target board is Raspberry Pi.
This driver uses 5 GPIOs on target board with a bit-bang SPI implementation
(hence, may not be as fast).
Makes use of WiringPI-library of Gordon Henderson (https://projects.drogon.net/raspberry-pi/wiringpi/)

Recommended connection (http://www.raspberrypi.org/archives/384):
LCD pins      Raspberry Pi
LCD1 - GND    P06  - GND
LCD2 - VCC    P01 - 3.3V
LCD3 - CLK    P11 - GPIO0
LCD4 - Din    P12 - GPIO1
LCD5 - D/C    P13 - GPIO2
LCD6 - CS     P15 - GPIO3
LCD7 - RST    P16 - GPIO4
LCD8 - LED    P01 - 3.3V

References  :
http://www.arduino.cc/playground/Code/PCD8544
http://ladyada.net/products/nokia5110/
http://code.google.com/p/meshphone/

================================================================================
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.
================================================================================
*/

#include <stdint.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>

#include "PCD8544.h"

/* the memory buffer for the LCD */
uint8_t pcd8544_buffer[LCDWIDTH * LCDHEIGHT / 8] = {0,};

void LCDInit(uint8_t contrast)
{
	/* get into the EXTENDED mode! */
	LCDcommand(PCD8544_FUNCTIONSET | PCD8544_EXTENDEDINSTRUCTION );

	/* LCD bias select (4 is optimal?) */
	LCDcommand(PCD8544_SETBIAS | 0x4);

	/* set VOP */
	if (contrast > 0x7f)
		contrast = 0x7f;

	LCDcommand( PCD8544_SETVOP | contrast); /* Experimentally determined */

	/* normal mode */
	LCDcommand(PCD8544_FUNCTIONSET);

	/* Set display to Normal */
	LCDcommand(PCD8544_DISPLAYCONTROL | PCD8544_DISPLAYNORMAL);
}

/* draw a rectangle */
void LCDdrawrect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)
{
	/* stupidest version - just pixels - but fast with internal buffer! */
	uint8_t i;
	for ( i=x; i<x+w; i++) {
		 LCDsetPixel(i, y, color);
		 LCDsetPixel(i, y+h-1, color);
	}
	for ( i=y; i<y+h; i++) {
		 LCDsetPixel(x, i, color);
		 LCDsetPixel(x+w-1, i, color);
	}
}

/* the most basic function, set a single pixel */
void LCDsetPixel(uint8_t x, uint8_t y, uint8_t color)
{
	if ((x >= LCDWIDTH) || (y >= LCDHEIGHT))
		return;

	/* x is which column */
	if (color)
		pcd8544_buffer[x+ (y/8)*LCDWIDTH] |= _BV(y%8);
	else
		pcd8544_buffer[x+ (y/8)*LCDWIDTH] &= ~_BV(y%8);
}

void LCDspiwrite(uint8_t c)
{
        uint8_t i;
        uint32_t j;

        for (i = 0; i < 8; i++)  {

		if (!!(c & (1 << (7 - i))) == 0)
			PORTC &= 0xEF;
		else
			PORTC |= 0x10;

		PORTC |= 0x20;    /* DC -> HIGH */

                for (j = 16; j > 0; j--); /* clock speed, anyone? (LCD Max CLK input: 4MHz) */

		PORTC &= 0xDF;    /* DC -> LOW */
        }
}

void LCDcommand(uint8_t c)
{
	PORTC &= 0xFE;    /* DC -> LOW */
	LCDspiwrite(c);
}

void LCDdata(uint8_t c)
{
	PORTC |= 0x01;    /* DC -> HIGH */
	LCDspiwrite(c);
}

void LCDdisplay(void)
{
	uint8_t col, maxcol, p;

	for(p = 0; p < 6; p++)
	{
		LCDcommand(PCD8544_SETYADDR | p);


		/* start at the beginning of the row */
		col = 0;
		maxcol = LCDWIDTH-1;

		LCDcommand(PCD8544_SETXADDR | col);

		for(; col <= maxcol; col++) {
			LCDdata(pcd8544_buffer[(LCDWIDTH*p)+col]);
		}
	}

	LCDcommand(PCD8544_SETYADDR );  /* no idea why this is necessary but it is to finish the last byte? */

}

/* clear everything */
void LCDclear(void) {
	/* memset(pcd8544_buffer, 0, LCDWIDTH*LCDHEIGHT/8); */
	uint32_t i;
	for ( i = 0; i < LCDWIDTH*LCDHEIGHT/8 ; i++)
		pcd8544_buffer[i] = 0;
}

