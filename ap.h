/*
 * ToRT - Toy-grade OSEK/VXD-inspired RTOS
 *
 * ap.h: Demo application: A simple Tetris clone
 *
 * Copyright (c) 2019, Helmut Sipos <helmut.sipos@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/

#ifndef AP_H
#define AP_H

#include <stdint.h>

/*******************************************
 * LCD hardware related configuration items
 *******************************************/

/* Should be understood as pixel on monochrome LCD "on" or "off". */
#define COLOR_BLACK			1
#define COLOR_WHITE			0

/* LCD 5110 sizes */
#define LCD_WIDTH			84
#define LCD_HEIGHT			48

/* Left and right offsets to have the action happen in the middle of
 * the display */
#define DISPLAY_OFFSET_X		2
#define DISPLAY_OFFSET_Y		2

/*************************************************
 * Tetris application related configuration items
 *************************************************/

/* Tetris game board. I selected 8 x 16 instead of the more common 10 x 20.
 * Rationale: Be able to easily represent the board as a matrix of bits
 * that nicely fits onto byte boundaries and therefore enables usage of
 * bit manipulation to model the game. README.md has the details
 */
#define BOARD_COLUMNS			8
#define BOARD_ROWS			16

/* There are seven types of tetrominones. See README.md for details */
#define TETROMINO_TYPES			7

/* Oriented of a tetromino. Basically the four cardinal directions */
#define TETROMINO_ORIENTATIONS		4

/* Normal falling speed: One second between advancements on the y coordinate */
#define SPEED_DEFAULT			250

/* Accelerated falling speed. After one press on the "drop" button. */
#define SPEED_FAST			50

/* Drop the tetromino with the next timer tick */
#define SPEED_ULTIMATE			1

/* The width of the square in which a tetromino fits */
#define TEROMINO_WIDTH			4

/* Board X center */
#define POSITION_X_CENTER		((BOARD_COLUMNS - TEROMINO_WIDTH) / 2)

/* Board Y positions min max */
#define POSITION_Y_TOP			0
#define POSITION_Y_BOTTOM		BOARD_ROWS

/* Number of pixels that make up the side length of a square */
#define SQUARE_SIDE_LENGTH		(LCD_WIDTH / BOARD_ROWS)

/* Number of bytes needed for the bit map representing a tetromino.
 * See README.md for details */
#define NR_BYTES_BITMAP			2

/* A completed row - all squares (bits) are occupied */
#define ROW_COMPLETED			0xFF
/* And none are occupied in an empty row */
#define ROW_EMPTY			0x00

/* Possible tetromino orientations in space */
enum {
         UP
        ,RIGHT
        ,DOWN
        ,LEFT
};

/* Describes the active (falling) tetromino */
typedef struct {
	/* Type of the falling tetromino (there are 7 types) */
        volatile uint8_t Type;
	/* Current orientation of the falling tetromino */
        volatile uint8_t Orientation;
	/* Falling speed. More precisely the time between
	 * an advancement on the y-position, in units of 4 ms */
        volatile uint8_t Speed;
	/* Location on the baord of the falling tetromino */
        volatile uint8_t Pos_x;
        volatile uint8_t Pos_y;
} ActiveTetromino;

/***********************************************
 * Operating system related configuration items
 **********************************************/


/* Identifiers of the used tasks. Must be unique */
#define TASK_ID_IDLE	           	0
#define TASK_ID_MODEL           	1
#define TASK_ID_VIEW            	2
#define TASK_ID_CTRL            	3

/* Size of the stack for each of the used tasks.
 * Tricky get the value right. Try using canaries at the end of the stack
 * and see if they get killed to find the right size. Without good tools
 * this is more or less a game of educated guessing (context size +
 * local variables + function calls + interrupts + ... ) plus try and error.
 * If you experience bizare, unexplainable behaviour after adding innocently
 * looking new code to a task try increasing this value and see if the problem
 * goes away.
 */
#define TASK_STACK_SIZE_IDLE            (TASK_STACK_SIZE_MIN + 32)
#define TASK_STACK_SIZE_VIEW		(TASK_STACK_SIZE_MIN + 64)
#define TASK_STACK_SIZE_MODEL		(TASK_STACK_SIZE_MIN + 128)
#define TASK_STACK_SIZE_CTRL		(TASK_STACK_SIZE_MIN + 128)

/* Priorities of the used tasks. Must be unique */
#define TASK_PRIORITY_IDLE              0
#define TASK_PRIORITY_MODEL             3
#define TASK_PRIORITY_VIEW              2
#define TASK_PRIORITY_CTRL              1

/* Identifiers of the used timer */
#define TIMER_ID_GAME                   0

/* Resources used. Bit encoded. There can be at most 8 resources PER SYSTEM */
#define RESOURCE_UART                   0x01
#define RESOURCE_LCD_SCREEN             0x02
#define RESOURCE_LCD_BACKLIGHT          0x04
#define RESOURCE_LED_RED                0x08
#define RESOURCE_LED_GREEN              0x10
#define RESOURCE_CONTROLS               0x20
#define RESOURCE_BOARD                  0x40

/* Events used. Bit encoded. There can be at most 8 events PER TASK */
#define EVENT_TIMER                     0x01
#define EVENT_UPDATE                    0x02
#define EVENT_DRAW                      0x04
#define EVENT_LEFT                      0x08
#define EVENT_RIGHT                     0x10
#define EVENT_ROTATE                    0x20
#define EVENT_DROP                      0x40

#endif /* AP_H */

