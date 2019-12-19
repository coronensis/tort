/*
 * ToRT - Tetris device with toy-grade OSEK/VDX-inspired RTOS
 *
 * emulator.c: Linux emulator for the "Tetris Device"
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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sysexits.h>
#include <pthread.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>

#define COLOR_BLACK			0
#define COLOR_WHITE			1

/* LCD 5110 sizes */
#define LCD_WIDTH			84
#define LCD_HEIGHT			48

/* Left and right offsets to have the action happen in the middle of
 * the display */
#define DISPLAY_OFFSET_X		2
#define DISPLAY_OFFSET_Y		2

/* Tetris game board. I selected 8 x 16 instead of the more common 10 x 20.
 * Rationale: Be able to easily represent the board as a matrix of bits
 * that nicely fits onto byte boundaries
 * README.md has the details
 */
#define BOARD_COLUMNS			8
#define BOARD_ROWS			16

/* There are seven types of tetrominones. See README.md for details */
#define TETROMINO_TYPES			7

/* Orientations of a tetromino. The four cardinal directions */
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

/* The dynamic object in this game. It is the falling tetromino that can be manipulated */
static ActiveTetromino Falling;

/* Keeps count of completed rows i.e. score. Only displayed via UART. You need
 * to run a terminal program connected to the microcontroller to see the values */
static uint8_t Score;

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
 * a tetromino sticking out of the board area or performing "rotations" where not sufficient
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

/* X11 stuff */
Display *Dpy;
GC Pen;
XGCValues Values;
XEvent Ev;

/* Mutexes and condition variable needed to control access to resources
 * and signal the view task to run */
pthread_mutex_t MutexBoard;
pthread_mutex_t MutexControl;
pthread_mutex_t MutexDraw;

pthread_cond_t CondDraw;

/* Draw a pixel */
static void LCDsetPixel(uint8_t x, uint8_t y, uint8_t color)
{
	if (color == COLOR_BLACK)
		XSetForeground(Dpy, Pen, BlackPixel(Dpy, DefaultScreen(Dpy)));
	else
		XSetForeground(Dpy, Pen, WhitePixel(Dpy, DefaultScreen(Dpy)));

	XDrawPoint(Dpy, Ev.xany.window, Pen, x, y);
}

/* Draw a rectangle */
static void LCDdrawrect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)
{
	uint8_t i;
	for ( i=x; i<x+w; i++) {
		LCDsetPixel(i, y, color);
		LCDsetPixel(i, y + h - 1, color);
	}
	for ( i=y; i<y+h; i++) {
		LCDsetPixel(x, i, color);
		LCDsetPixel(x + w - 1, i, color);
	}

}

/* Clear the LCD display - the X11 window in this case */
static void LCDclear(void)
{
	XClearWindow(Dpy, Ev.xany.window);
}

/* Flush the buffer to the display */
static void LCDdisplay(void)
{
	XFlush(Dpy);
}

/* Initiate a new falling tetromino */
static void NewTetromino(void)
{
	/* "randomly" select the new tetromino type */
	Falling.Type = (uint8_t) (rand() / (RAND_MAX / TETROMINO_TYPES + 1));
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
        uint8_t Line0 = 0, Line1 = 0, Line2 = 0, Line3 = 0;
        uint8_t Detected = 0;

	/* A tetromino is represented as a 4x4 bitmap that has the bits set that make up
	 * the tetromino. Detecting a collision resumes to checking if there is any bit
	 * superposition between the tetromino's line and the line of the board.
	 * Take into account that a tetromino falls line by line starting at 0 so only check
	 * the lines that are already on the boad.
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

/* Place a tetromino on the board */
static void AddTetromino(uint8_t Type, uint8_t Orientation, uint8_t Pos_x, uint8_t Pos_y)
{
	/* Take into account that a tetromino falls line by line starting at 0 so only add
	 * the lines that should go the boad according to the current y position of the
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

static int X11EventLoop (void)
{
	for(;;) {
		XNextEvent(Dpy, &Ev);

		/* Block tasks that compete for the "control" resource */
		pthread_mutex_lock (&MutexControl);

		/* Temporarily remove current active tetromino from the board to avoid detecting false
		 * collisions with itself */
                RemoveTetromino(Falling.Type, Falling.Orientation, Falling.Pos_x, Falling.Pos_y);

		switch(Ev.type) {
		case Expose:
			break;

		case MapNotify:
			break;

		case KeyRelease:

			switch (XkbKeycodeToKeysym(Dpy, Ev.xkey.keycode, 0, Ev.xkey.state & ShiftMask ? 1 : 0)) {
			case XK_Up: {
					    uint8_t Orientation = Falling.Orientation;
					    Orientation++;

					    /* Orientation is cycled between the 4 allowed values */
					    Orientation %= TETROMINO_ORIENTATIONS;

					    if (!DetectCollision(Falling.Type, Orientation, Falling.Pos_x, Falling.Pos_y)) {
						    Falling.Orientation = Orientation;
					    }
				    }
				break;

			case XK_Down:
				/* Each new tetromino defaults to "normal" falling speed */
				if (Falling.Speed == SPEED_DEFAULT) {
					/* With the first push set the "fast" falling speed. */
					Falling.Speed = SPEED_FAST;
				} else if (Falling.Speed == SPEED_FAST) {
					/* With the second push just drop the tetromino */
					Falling.Speed = SPEED_ULTIMATE;
				}
				break;

			case XK_q:
				XCloseDisplay(Dpy);
				exit(0);
			}
			break;

		case ButtonPress:
			switch (Ev.xbutton.button) {
			case Button4:
				/* Validate the motion request. X must be within allowed bounds
				 * for tetromino type and orientation */
				if (Falling.Pos_x < Max_Pos_x[Falling.Type][Falling.Orientation]) {
					/* And no collision must occur by the requested motion */
					if (!DetectCollision(Falling.Type, Falling.Orientation, Falling.Pos_x + 1, Falling.Pos_y)) {
						Falling.Pos_x++;
					}
				}
				break;

			case Button5:
				if (Falling.Pos_x > 0) {
					if (!DetectCollision(Falling.Type, Falling.Orientation, Falling.Pos_x - 1, Falling.Pos_y)) {
						Falling.Pos_x--;
					}
				}
				break;
			}
		}

		pthread_mutex_unlock (&MutexControl);

		/* Put back the temporarily removed tetromino */
                AddTetromino(Falling.Type, Falling.Orientation, Falling.Pos_x, Falling.Pos_y);
	}
}

/* This task is responsible for translating the Tetris board to a 2D image and sending it to
 * the LCD display. It is triggered by the DRAW event */
static void *TaskView(void *arg)
{
	uint8_t Row, Col, Width, Height;

	(void)arg;

	for (;;) {

		/* Wait until drawing is requested */
                pthread_mutex_lock (&MutexDraw);
                pthread_cond_wait (&CondDraw, &MutexDraw);
                pthread_mutex_unlock (&MutexDraw);

		/* Clear the screen */
		LCDclear();

		/* Draw a rectangle around the "playing area" */
		LCDdrawrect(2, 2, LCD_WIDTH - 4 , LCD_HEIGHT - 7, COLOR_BLACK);

		/* Do not modify the board while the display buffer is constructed from it */
		pthread_mutex_lock (&MutexBoard);

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

		pthread_mutex_unlock (&MutexBoard);

		/* Send the display buffer to the LCD display */
		LCDdisplay();
	}

	return NULL;
}

static void *TaskModel(void *arg)
{
	(void)arg;

	/* sync w/ main */
	sleep (1);

	for (;;) {

		/* Inhibit other taks using these resources while this task uses them */
		pthread_mutex_lock (&MutexControl);
		pthread_mutex_lock (&MutexBoard);

		/* Remove the tetromino as it was before from the board */
                if (Falling.Pos_y < (POSITION_Y_BOTTOM - 1))
                        RemoveTetromino(Falling.Type, Falling.Orientation, Falling.Pos_x, Falling.Pos_y);

		/* Update the y position of the falling tetromino */
                Falling.Pos_y++;

		/* Check for a collition with the new Y position */
                if (DetectCollision(Falling.Type, Falling.Orientation, Falling.Pos_x, Falling.Pos_y)) {

			/* Put the previously remove tetromino back to be able to
			 * check for completed rows */
                        AddTetromino(Falling.Type, Falling.Orientation, Falling.Pos_x, Falling.Pos_y - 1);

			/* See if a row was completed and the score needs to
			 * be updated and the green LED lit */
                        CheckCompletedRows();

                        /* Initialize a new tetromino */
			NewTetromino();

                        /* If a collision on board line 0 happens with the newly
			 * initialized tetromino, the game has ended */
                        if (DetectCollision(Falling.Type, Falling.Orientation, Falling.Pos_x, Falling.Pos_y)) {
                                printf("Game Over!\nStarting new game...\n");
                                memset(Board, 0x00, sizeof(Board));
                                Score = 0;
                        }
                }

		/* Add the updated tetromino to the board */
		AddTetromino(Falling.Type, Falling.Orientation, Falling.Pos_x, Falling.Pos_y);

		/* Release resources */
		pthread_mutex_unlock (&MutexBoard);
		pthread_mutex_unlock (&MutexControl);

		/* Trigger the view tasks to draw the updated board to the LCD
		 * display */
                pthread_mutex_lock (&MutexDraw);
                pthread_cond_broadcast (&CondDraw);
                pthread_mutex_unlock (&MutexDraw);

		/* Time between advancing the falling tetromino */
		usleep(Falling.Speed * 4000);
	}

	return NULL;
}

int main(int argc, char ** argv)
{
	int ScreenNum;
	unsigned long Background, Border;
	struct timeval Tv;
	Window Win;

        pthread_t thTaskView;
        pthread_t thTaskModel;
        pthread_attr_t attrTaskView;
        pthread_attr_t attrTaskModel;

	(void)argc;
	(void)argv;

	printf ("Keyboard 'q' quits the emulator\n");
	printf ("Keyboard 'Up' rotates the teromino\n");
	printf ("Keyboard 'Down' drops the teromino\n");
	printf ("Mouse wheel 'Up' moves the teromino to the left\n");
	printf ("Mouse wheel 'Down' moves the teromino to the right\n");

	/* Seed the RNG */
	gettimeofday(&Tv, NULL);
	srand(Tv.tv_usec);

	Score = 0;

	/* Initialize the board */
        memset(Board, 0, sizeof(Board));

	/* Initialize a new tetromino to be dropped */
	NewTetromino();

	/* First connect to the display server */
	Dpy = XOpenDisplay(NULL);
	if (!Dpy) {
		fprintf(stderr, "unable to connect to display\n");
		return EX_SOFTWARE;
	}

	/* These are macros that pull useful data out of the display object */
	/* we use these bits of info enough to want them in their own variables */
	ScreenNum = DefaultScreen(Dpy);
	Background = WhitePixel(Dpy, ScreenNum);
	Border = BlackPixel(Dpy, ScreenNum);

	Win = XCreateSimpleWindow(Dpy, DefaultRootWindow(Dpy), /* display, parent */
			0,0, /* x, y: the window manager will place the window elsewhere */
			LCD_WIDTH, LCD_HEIGHT, /* width, height */
			2, Border, /* Border width & colour, unless you have a window manager */
			Background); /* Background colour */

	/* Tell the display server what kind of events we would like to see */
	XSelectInput(Dpy, Win, ButtonPressMask | StructureNotifyMask | ExposureMask | KeyReleaseMask);

	/* Create the pen to draw lines with */
	Values.foreground = BlackPixel(Dpy, ScreenNum);
	Values.line_width = 1;
	Values.line_style = LineSolid;
	Pen = XCreateGC(Dpy, Win, GCForeground|GCLineWidth|GCLineStyle,&Values);

	/* Put the window on the screen */
	XMapWindow(Dpy, Win);

	/* Initialize mutexes */
        pthread_mutex_init(&MutexBoard, NULL);
	pthread_mutex_init(&MutexControl, NULL);
	pthread_mutex_init(&MutexDraw, NULL);

	/* Initialize condition variable */
        pthread_cond_init (&CondDraw, NULL);

	/* Create the view "task" */
	pthread_attr_init (&attrTaskView);
        if (pthread_create (&thTaskView, &attrTaskView, TaskView, NULL)) {
		printf ("could not create thread TaskView: %s", strerror (errno));
		exit (EX_OSERR);
	}

	/* Create the model "task" */
	pthread_attr_init (&attrTaskModel);
        if (pthread_create (&thTaskModel, &attrTaskModel, TaskModel, NULL)) {
		printf ("could not create thread TaskView: %s", strerror (errno));
		exit (EX_OSERR);
	}

	/* Handle X11 events. Does not return */
	X11EventLoop();

	/* Never reached */
	return EX_OK;
}

