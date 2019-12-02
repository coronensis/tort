/*
 * ToRT - Toy-grade OSEK/VXD-inspired RTOS
 *
 * ap.c: Demo application: A simple Tetris clone
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <avr/sleep.h>

#include "os.h"
#include "ap.h"
#include "PCD8544.h"

/* Keeps count of completed rows i.e. the score. Only displayed via UART. You need
 * to run a terminal program connected to the microcontroller to see the values */
static uint8_t Score;

/* The dynamic object in this game. It is the falling tetromino that can be manipulated */
static ActiveTetromino Falling;

/* The Tetris board. Modeled as 16 rows (bytes) times 8 columns (bits) */
static uint8_t Board[BOARD_ROWS] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/* All possible tetromino types in all possible orientations. Encoded as bitmaps. */
static const uint8_t Tetrominoes[TETROMINO_TYPES][TETROMINO_ORIENTATIONS][NR_BYTES_BITMAP] = {
         {{0x00, 0x47}, {0x03, 0x22}, {0x00,0x71}, {0x01, 0x13}}
        ,{{0x00, 0x63}, {0x01, 0x32}, {0x00,0x63}, {0x01, 0x32}}
        ,{{0x00, 0x17}, {0x02, 0x23}, {0x00,0x74}, {0x03, 0x11}}
        ,{{0x00, 0x36}, {0x02, 0x31}, {0x00,0x36}, {0x02, 0x31}}
        ,{{0x00, 0x0F}, {0x11, 0x11}, {0x00,0x0F}, {0x11, 0x11}}
        ,{{0x00, 0x33}, {0x00, 0x33}, {0x00,0x33}, {0x00, 0x33}}
        ,{{0x00, 0x27}, {0x02, 0x32}, {0x00,0x72}, {0x01, 0x31}}
};

/* Depending on the tetromino type and orientation the max. x position varies to avoid
 * tetromino sticking out of the board area or performing "rotations" where not sufficient
 * space is available */
static const uint8_t Max_Pos_x[TETROMINO_TYPES][TETROMINO_ORIENTATIONS] = {
          {5, 6, 5, 6}
         ,{5, 6, 5, 6}
         ,{5, 6, 5, 6}
         ,{5, 6, 5, 6}
         ,{4, 7, 4, 7}
         ,{6, 6, 6, 6}
         ,{5, 6, 5, 6}
};

/* Statically allocate space for the stack of each task */
static uint8_t TaskStackIdle[TASK_STACK_SIZE_IDLE];
static uint8_t TaskStackModel[TASK_STACK_SIZE_MODEL];
static uint8_t TaskStackView[TASK_STACK_SIZE_VIEW];
static uint8_t TaskStackCtrl[TASK_STACK_SIZE_CTRL];

/* Scheduling table. Set up the task descriptors. */
static TaskDescriptor Tasks[] = {
	{
		 &TaskStackIdle[TASK_STACK_SIZE_IDLE - 1 - SIZE_SAVED_CONTEXT]
		,TASK_STATE_READY
		,EVENT_NONE
		,EVENT_NONE
		,RESOURCE_NONE
		,TASK_PRIORITY_IDLE
	}
	,{
		 &TaskStackModel[TASK_STACK_SIZE_MODEL - 1 - SIZE_SAVED_CONTEXT]
		,TASK_STATE_READY
		,EVENT_NONE
		,EVENT_NONE
		,RESOURCE_CONTROLS | RESOURCE_BOARD | RESOURCE_UART
		,TASK_PRIORITY_MODEL
	}
	,{
		&TaskStackView[TASK_STACK_SIZE_VIEW - 1 - SIZE_SAVED_CONTEXT]
		,TASK_STATE_READY
		,EVENT_NONE
		,EVENT_NONE
		,RESOURCE_BOARD | RESOURCE_LCD_SCREEN
		,TASK_PRIORITY_VIEW
	}
	,{
		&TaskStackCtrl[TASK_STACK_SIZE_CTRL - 1 - SIZE_SAVED_CONTEXT]
		,TASK_STATE_READY
		,EVENT_NONE
		,EVENT_NONE
		,RESOURCE_CONTROLS
		,TASK_PRIORITY_CTRL
	}
};

#define NR_TASKS (sizeof(Tasks)/sizeof(TaskDescriptor))

/* Only one application timer is needed to drive the falling of the active tetromino */
static TimerDescriptor Timers[] = {
        {
                 0
                ,TASK_ID_MODEL
                ,EVENT_TIMER
        }
};

/* Initiate a new falling tetromino */
static void NewTetromino(void)
{
	/* "randomly" select the new tetromino type */
	Falling.Type = rand() / (RAND_MAX / TETROMINO_TYPES + 1);
	/* Defaults for each new tetromino */
	Falling.Orientation = UP;
	Falling.Speed = SPEED_DEFAULT;
	Falling.Pos_x = POSITION_X_CENTER;
	Falling.Pos_y = POSITION_Y_TOP;
}

/* Detect if the tetromino collides with any element of the board,
 * i.e. other tetrominoes that previously fell or the boards' limits */
static uint8_t DetectCollision(uint8_t Type, uint8_t Orientation, uint8_t Pos_x, uint8_t Pos_y)
{
	/* README.md describes the representation of a tetromino */
        uint8_t Line0 = 0, Line1 = 0, Line2 = 0, Line3 = 0;
        uint8_t Detected = 0;

	/* A tetromino is represented as a 4x4 bitmap that has the bits set that represent
	 * the tetromino. Detecting a collision resumes to checking if there is any bit
	 * superposition between the tetromino's line and the line of the board.
	 * Take into account that a tetromino falls line by line starting at 0 so only check
	 * the lines that are on the boad.
	 */
        Line0 = Board[Pos_y] & ((Tetrominoes[Type][Orientation][1] & 0x0F) << Pos_x);
        if (Pos_y > 0)
                Line1 = Board[Pos_y - 1] & ((Tetrominoes[Type][Orientation][1] >> 4) << Pos_x);
        if (Pos_y > 1)
                Line2 = Board[Pos_y - 2] & ((Tetrominoes[Type][Orientation][0] & 0x0F) << Pos_x);
        if (Pos_y > 2)
                Line3 = Board[Pos_y - 3] & ((Tetrominoes[Type][Orientation][0] >> 4) << Pos_x);
        if (Line0 || Line1 || Line2 || Line3) {
                Detected = 1;
	}
	else if ((Pos_x > Max_Pos_x[Type][Orientation]) || (Pos_y > (POSITION_Y_BOTTOM - 1))) {
                Detected = 1;
	}

        return Detected;
}

/* Place/drop a tetromino on the board */
static void AddTetromino(uint8_t Type, uint8_t Orientation, uint8_t Pos_x, uint8_t Pos_y)
{
	/* Take into account that a tetromino falls line by line starting at 0 so only add
	 * the lines that should go to the boad according to the current y position of the
	 * tetromino
	 */
        Board[Pos_y] |= (Tetrominoes[Type][Orientation][1] & 0x0F) << Pos_x;
        if (Pos_y > 0)
                Board[Pos_y - 1] |= (Tetrominoes[Type][Orientation][1] >> 4) << Pos_x;
        if (Pos_y > 1)
                Board[Pos_y - 2] |= (Tetrominoes[Type][Orientation][0] & 0x0F) << Pos_x;
        if (Pos_y > 2)
                Board[Pos_y - 3] |= (Tetrominoes[Type][Orientation][0] >> 4) << Pos_x;
}

/* Remove a tetromino from the board */
static void RemoveTetromino(uint8_t Type, uint8_t Orientation, uint8_t Pos_x, uint8_t Pos_y)
{
	/* Take into account that a tetromino falls line by line starting at 0 so only
	 * remove the lines on the boad that are already visible according to the current
	 * y position of the tetromino
	 */
        Board[Pos_y] &= ~((Tetrominoes[Type][Orientation][1] & 0x0F) << Pos_x);
        if (Pos_y > 0)
                Board[Pos_y - 1] &= ~((Tetrominoes[Type][Orientation][1] >> 4) << Pos_x);
        if (Pos_y > 1)
                Board[Pos_y - 2] &= ~((Tetrominoes[Type][Orientation][0] & 0x0F) << Pos_x);
        if (Pos_y > 2)
                Board[Pos_y - 3] &= ~((Tetrominoes[Type][Orientation][0] >> 4) << Pos_x);
}

/* Check for and remove completed rows, increment the score for each completed row found */
static void CheckCompletedRows(void)
{
        uint8_t Row, RemainingRows;

        for (Row = 0; Row < BOARD_ROWS; Row++) {
                if (Board[Row] == ROW_COMPLETED) {
			/* Yay! */
                        Score++;

			/* Flash green LED for successful completion of a row */
			Os_LEDGreenOn();

			/* And report via UART */
                        printf("Score: %d\n", Score);

			/* Move the remaining rows towards the bottom */
                        for (RemainingRows = Row; RemainingRows > 0; RemainingRows--) {
                                Board[RemainingRows] = Board[RemainingRows - 1];
                                if (Board[RemainingRows - 1] == ROW_EMPTY)
                                        break;
                        }
                        Board[0] = ROW_EMPTY;
                }
        }
}

/* This task is responsible for translating the Tetris board to a 2D image and sending it to
 * the LCD display. It is triggered by the DRAW event */
static void TaskView(void)
{
	uint8_t Row, Col, Width, Height;

	for (;;) {
		Os_WaitEvents(EVENT_DRAW);
		Os_ClearEvents(EVENT_DRAW);

		/* Clear the screen */
		LCDclear();

		/* Draw a rectangle around the playing area */
		LCDdrawrect(2, 2, LCD_WIDTH - 4 , LCD_HEIGHT - 7, COLOR_BLACK);

		/* Do not modify the board while the display buffer is constructed from it */
		Os_GetResources(RESOURCE_BOARD);

		/* Scan the board and translate it to graphic objects to be displayed.
		 * Both the board and the tetrominos are made up of "squares" that are
		 * drawn as N x N squares on the display */
		for (Row = 0; Row < BOARD_ROWS; Row++) {
			for (Col = 0;  Col  < BOARD_COLUMNS; Col++) {
				if (Board[Row] & (1 << Col)) {
					for (Width = 0; Width < SQUARE_SIDE_LENGTH; Width++) {
						for (Height = 0; Height < SQUARE_SIDE_LENGTH; Height++) {
							LCDsetPixel(DISPLAY_OFFSET_X + (Row * SQUARE_SIDE_LENGTH) + Width
								    ,DISPLAY_OFFSET_Y + (Col * SQUARE_SIDE_LENGTH) + Height
								    ,COLOR_BLACK
								    );
						}
					}
				}
			}
		}

		Os_ReleaseResources(RESOURCE_BOARD);

		/* Send the display buffer to the LCD display */
		LCDdisplay();
	}
}

/* The task handles the "model" of the game. It manages the Tetris board,
 * checks for "game over" situtations, triggers updating the display,
 * keeps count of completed rows and initializes new falling tetrominoes
 * as needed
 */
static void TaskModel(void)
{
	for (;;) {

		/* Changes happen either because of the timer that drives the
		 * falling of the tetromino OR because of user interaction.
		 * Wait for one of those events before doing anything */
		Os_WaitEvents(EVENT_TIMER | EVENT_UPDATE);

		/* Put the LEDs out in case they were on after a game restart
		 * of completed line */
		Os_LEDGreenOff();
		Os_LEDRedOff();

		/* Inhibit other taks using these resources while this task uses
		 * them */
		Os_GetResources(RESOURCE_CONTROLS | RESOURCE_BOARD);

		/* User controls triggered this execution round */
		if (Os_GetEvents() & EVENT_UPDATE)
			Os_ClearEvents(EVENT_UPDATE);

		/* Remove the tetromino as it was before from the board */
                if (Falling.Pos_y < (POSITION_Y_BOTTOM - 1))
                        RemoveTetromino(Falling.Type, Falling.Orientation, Falling.Pos_x, Falling.Pos_y);

		/* If the timer triggered this task execution round update the
		 * y position of the falling tetromino */
		if (Os_GetEvents() & EVENT_TIMER) {
	                Falling.Pos_y++;
			Os_ClearEvents(EVENT_TIMER);
		}

		/* Check for a collision with the new Y position */
                if (DetectCollision(Falling.Type, Falling.Orientation, Falling.Pos_x, Falling.Pos_y)) {

			/* Put the previously removed tetromino back to be able to
			 * check for completed rows */
                        AddTetromino(Falling.Type, Falling.Orientation, Falling.Pos_x, Falling.Pos_y - 1);

			/* See if a row was completed and the score needs to
			 * be updated and the green LED lit */
                        CheckCompletedRows();

                        /* Initialize a new tetromino */
			NewTetromino();

                        /* If a collision on the board at line 0 happens with the newly
			 * dropping tetromino, the game has ended */
                        if (DetectCollision(Falling.Type, Falling.Orientation, Falling.Pos_x, Falling.Pos_y)) {
				Os_LEDRedOn();
                                printf("Game Over!\nStarting new game...\n");
                                memset(Board, 0x00, sizeof(Board));
                                Score = 0;
                        }
                }

		/* Add the updated tetromino to the board */
		AddTetromino(Falling.Type, Falling.Orientation, Falling.Pos_x, Falling.Pos_y);

		Os_ReleaseResources(RESOURCE_BOARD | RESOURCE_CONTROLS);

		/* Schedule timer event to happen according to the selected
		 * falling speed in order to update the position of the falling
		 * tetromino */
		Os_SetTimer(TIMER_ID_GAME, Falling.Speed);

		/* Trigger the view tasks to draw the updated board to the LCD
		 * display */
		Os_SetEvent(TASK_ID_VIEW, EVENT_DRAW);
	}
}

/* This task handles input from the different control "devices". Validates the requests
 * and signals the "model" task if any update is required
 */
static void TaskCtrl(void)
{
	uint8_t Event = 0;
	uint8_t Updated = 0;

	for (;;) {

		/* Wait for any of the events generated by the controllers */
		Os_WaitEvents(  EVENT_LEFT
			      | EVENT_RIGHT
			      | EVENT_ROTATE
			      | EVENT_DROP
			      );

		Event = Os_GetEvents();

		/* Block tasks that compete for the "control" resource */
		Os_GetResources(RESOURCE_CONTROLS);

		/* Temporarily remove current active tetromino from the board to avoid detecting false
		 * collisions with itself */
                RemoveTetromino(Falling.Type, Falling.Orientation, Falling.Pos_x, Falling.Pos_y);

		Updated = 0;

		/* Event from the potentiometer to move the falling tetromino left */
		if (Event & EVENT_LEFT) {
			/* Validate the motion request. X must be within allowed bounds
			 * for tetromino type and orientation */
			if (Falling.Pos_x < Max_Pos_x[Falling.Type][Falling.Orientation]) {
				/* And no collision must occur by the requested motion */
				if (!DetectCollision(Falling.Type, Falling.Orientation, Falling.Pos_x + 1, Falling.Pos_y)) {
					Falling.Pos_x++;
					Updated = 1;
				}
			}
			Os_ClearEvents(EVENT_LEFT);
		}

		/* Event from the potentiometer to move the falling tetromino right */
		if (Event & EVENT_RIGHT) {
			if (Falling.Pos_x > 0) {
				if (!DetectCollision(Falling.Type, Falling.Orientation, Falling.Pos_x - 1, Falling.Pos_y)) {
					Falling.Pos_x--;
					Updated = 1;
				}
			}
			Os_ClearEvents(EVENT_RIGHT);
		}

		/* Event from the 'Rotate' button to change the orientation of the falling tetromino */
		if (Event & EVENT_ROTATE) {
			uint8_t Orientation = Falling.Orientation;
			Orientation++;

			/* Orientation is cycled between the 4 allowed values */
			Orientation %= TETROMINO_ORIENTATIONS;

			if (!DetectCollision(Falling.Type, Orientation, Falling.Pos_x, Falling.Pos_y)) {
				Falling.Orientation = Orientation;
				Updated = 1;
			}
			Os_ClearEvents(EVENT_ROTATE);
		}

		/* Event from the 'Drop' button to change the falling speed of te tetromino */
		if (Event & EVENT_DROP) {
			/* Each new tetromino defaults to "normal" falling speed */
			if (Falling.Speed == SPEED_DEFAULT) {
				/* With the first push set the "fast" falling speed. */
				Falling.Speed = SPEED_FAST;
			} else if (Falling.Speed == SPEED_FAST) {
				/* With the second push just drop the tetromino */
				Falling.Speed = SPEED_ULTIMATE;
			}
			Os_ClearEvents(EVENT_DROP);
		}

		Os_ReleaseResources(RESOURCE_CONTROLS);

		/* Put back the remporarily removed tetromino */
                AddTetromino(Falling.Type, Falling.Orientation, Falling.Pos_x, Falling.Pos_y);

		/* If any change happened to the falling tetromino trigger the "model" task to
		 * update the board and subsequently the dispay */
		if (Updated)
			Os_SetEvent(TASK_ID_MODEL, EVENT_UPDATE);
	}
}

/* This task runs when no other task is active.
 * It could be used for example to measure how much CPU time resources are left on the system */
static void TaskIdle(void)
{
	for (;;) {
		/* Turn off the processor core, the rest remains active */
		set_sleep_mode(SLEEP_MODE_IDLE);
		sleep_mode();
	}
}

/* Set up the task stacks so the context of the respective task can be restored from them
 * and the execution be started/resumed at the right place */
static void InitializeTaskStacks(void)
{
	/* Initialize task stacks to zero. The registers fetched from them upon a
	 * first context switch will be zero */
        memset(TaskStackIdle, 0, sizeof(TaskStackIdle));
        memset(TaskStackModel, 0, sizeof(TaskStackModel));
        memset(TaskStackView, 0, sizeof(TaskStackView));
        memset(TaskStackCtrl, 0, sizeof(TaskStackCtrl));

	/* Set the address of the idle task's function.
	 * This will be the execution entry point for this task as soon as it runs for the
	 * first time */
	TaskStackIdle[TASK_STACK_SIZE_IDLE - 1] = (uint8_t)((uint16_t)TaskIdle);
	TaskStackIdle[TASK_STACK_SIZE_IDLE - 2] = (uint8_t)((uint16_t)TaskIdle >> 8);

	/* Set the address of the model task's function */
	TaskStackModel[TASK_STACK_SIZE_MODEL - 1] = (uint8_t)((uint16_t)TaskModel);
	TaskStackModel[TASK_STACK_SIZE_MODEL - 2] = (uint8_t)((uint16_t)TaskModel >> 8);

	/* Set the address of the view task's function */
	TaskStackView[TASK_STACK_SIZE_VIEW - 1] = (uint8_t)((uint16_t)TaskView);
	TaskStackView[TASK_STACK_SIZE_VIEW - 2] = (uint8_t)((uint16_t)TaskView >> 8);

	/* Set the address of the controller task's function */
	TaskStackCtrl[TASK_STACK_SIZE_CTRL - 1] = (uint8_t)((uint16_t)TaskCtrl);
	TaskStackCtrl[TASK_STACK_SIZE_CTRL - 2] = (uint8_t)((uint16_t)TaskCtrl >> 8);
}

/* Helper function for directing the stdout file descriptor to the UART
 * this enables using functions like PRINTF(3) from the AVR libc */
static int SendChar(char c, FILE *stream)
{
	(void)stream;

	Os_UARTSend(c);

	return 0;
}

int main(void)
{
	/* Direct stdout to the microcontroller's UART port */
	FILE UartDebug = FDEV_SETUP_STREAM(SendChar, NULL, _FDEV_SETUP_WRITE);
	stdout = stderr = &UartDebug;

	/* Useful to detect involutary restarts */
	printf ("SYSTEM STARTUP\n");

        Score = 0;

	/* Disable all interrupts */
        Os_DisableAllInterrupts();

        /* Initialize the hardware */
        Os_HardwareInit();

	/* Call the Adafruit library init routine
	* LCD contrast = 60 */
	LCDInit(60);

	/* Turn on the LCD's backlight LEDs */
	Uc_LCDBacklightOn();

	/* Initialize the Tetris board */
        memset(Board, 0, sizeof(Board));

	/* Initialize a new tetromino to be dropped */
	NewTetromino();

	/* Initialize the stacks of the tasks */
	InitializeTaskStacks();

        /* Start the operating system */
        Os_StartOS(Tasks, NR_TASKS, Timers);

	/* This should never be reached */
	Os_ShutdownOS();

	/* Pacify the compiler */
	return 0;
}

