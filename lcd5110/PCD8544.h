/*
   =================================================================================
Name        : PCD8544.h
Version     : 0.1

Copyright (C) 2010 Limor Fried, Adafruit Industries
CORTEX-M3 version by Le Dang Dung, 2011 LeeDangDung@gmail.com (tested on LPC1769)
Raspberry Pi version by Andre Wussow, 2012, desk@binerry.de

Description : PCD8544 LCD library!

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

#define BLACK 1
#define WHITE 0

#define LCDWIDTH 84
#define LCDHEIGHT 48

#define PCD8544_EXTENDEDINSTRUCTION 0x01

#define PCD8544_DISPLAYNORMAL 0x4

/* H = 0 */
#define PCD8544_FUNCTIONSET 0x20
#define PCD8544_DISPLAYCONTROL 0x08
#define PCD8544_SETYADDR 0x40
#define PCD8544_SETXADDR 0x80

/* H = 1 */
#define PCD8544_SETBIAS 0x10
#define PCD8544_SETVOP 0x80

void LCDInit(uint8_t contrast);
void LCDcommand(uint8_t c);
void LCDdata(uint8_t c);
void LCDclear(void);
void LCDdisplay(void);
void LCDsetPixel(uint8_t x, uint8_t y, uint8_t color);
void LCDdrawrect(uint8_t x, uint8_t y, uint8_t w, uint8_t h,uint8_t color);
void LCDspiwrite(uint8_t c);
