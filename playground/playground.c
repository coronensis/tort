/*
 * TORT - Toy-grade RTOS with Tetris on top
 *
 * playground.c: Playground for preemptive scheduling. Uses the AVR simulator.
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
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <avr/interrupt.h>
#include <util/delay.h>

/* Choose this to be sufficently large to accomodate the context and other
 * stack usages (function calls, recursion, interrupts, etc.) by the target task */
#define TASK_STACK_SIZE 256

/* Arbitraily chosen */
#define DELAY_MAX_MS 10

/* Minimal "task descriptor" */
typedef struct {
        /* The stack reserved for this task */
        uint8_t *StackPointer;
} Task;

/* Save the context of the current task */
#define UC_SaveContext() \
	asm volatile ("push r0");\
	asm volatile ("in   r0, __SREG__");\
	asm volatile ("cli");\
	asm volatile ("push r0");\
	asm volatile ("push r1");\
	asm volatile ("clr  r1");\
	asm volatile ("push r2");\
	asm volatile ("push r3");\
	asm volatile ("push r4");\
	asm volatile ("push r5");\
	asm volatile ("push r6");\
	asm volatile ("push r7");\
	asm volatile ("push r8");\
	asm volatile ("push r9");\
	asm volatile ("push r10");\
	asm volatile ("push r11");\
	asm volatile ("push r12");\
	asm volatile ("push r13");\
	asm volatile ("push r14");\
	asm volatile ("push r15");\
	asm volatile ("push r16");\
	asm volatile ("push r17");\
	asm volatile ("push r18");\
	asm volatile ("push r19");\
	asm volatile ("push r20");\
	asm volatile ("push r21");\
	asm volatile ("push r22");\
	asm volatile ("push r23");\
	asm volatile ("push r24");\
	asm volatile ("push r25");\
	asm volatile ("push r26");\
	asm volatile ("push r27");\
	asm volatile ("push r28");\
	asm volatile ("push r29");\
	asm volatile ("push r30");\
	asm volatile ("push r31");\
	asm volatile ("lds  r26, CurrentTask");\
	asm volatile ("lds  r27, CurrentTask + 1");\
	asm volatile ("in   r0, __SP_L__");\
	asm volatile ("st   x+, r0");\
	asm volatile ("in   r0, __SP_H__");\
	asm volatile ("st   x+, r0");

/* Restore the context of the selected CurrentTask */
#define UC_RestoreContext() \
	asm volatile ("lds r26, CurrentTask");\
	asm volatile ("lds r27, CurrentTask + 1");\
	asm volatile ("ld  r28, x+");\
        asm volatile ("out __SP_L__, r28");\
	asm volatile ("ld  r29, x+");\
        asm volatile ("out __SP_H__, r29");\
        asm volatile ("pop r31");\
        asm volatile ("pop r30");\
        asm volatile ("pop r29");\
        asm volatile ("pop r28");\
        asm volatile ("pop r27");\
        asm volatile ("pop r26");\
        asm volatile ("pop r25");\
        asm volatile ("pop r24");\
        asm volatile ("pop r23");\
        asm volatile ("pop r22");\
        asm volatile ("pop r21");\
        asm volatile ("pop r20");\
        asm volatile ("pop r19");\
        asm volatile ("pop r18");\
        asm volatile ("pop r17");\
        asm volatile ("pop r16");\
        asm volatile ("pop r15");\
        asm volatile ("pop r14");\
        asm volatile ("pop r13");\
        asm volatile ("pop r12");\
        asm volatile ("pop r11");\
        asm volatile ("pop r10");\
        asm volatile ("pop r9");\
        asm volatile ("pop r8");\
        asm volatile ("pop r7");\
        asm volatile ("pop r6");\
        asm volatile ("pop r5");\
        asm volatile ("pop r4");\
        asm volatile ("pop r3");\
        asm volatile ("pop r2");\
        asm volatile ("pop r1");\
        asm volatile ("pop r0");\
        asm volatile ("out __SREG__, r0");\
        asm volatile ("pop r0");


/* Stack allocation for the tasks */
static uint8_t StackTaskOne[TASK_STACK_SIZE];
static uint8_t StackTaskTwo[TASK_STACK_SIZE];

/* "Scheduling table" */
static Task Tasks[] = {
        {
		/* The stack grows downwards so point to the end of the
		 * memory allocated or it. Subtract the size of the context.
		 * A task will start by restoring its context from there.
		 */
		&StackTaskOne[TASK_STACK_SIZE - 36]
        }
        ,{
		&StackTaskTwo[TASK_STACK_SIZE - 36]
        }
};

#define NR_TASKS (sizeof (Tasks) / sizeof (Task))

/* Will contain the stack pointer of the main context */
uint8_t MainContextSP[2];

/* The main() context descriptor. Only care about the stack pointer */
Task Main = {&MainContextSP[0]};

/* Saving the task context will write the current main context's stack pointer
 * to the above buffer. NULL would mean killing stuff where the reset vector
 * points to */
Task volatile *CurrentTask = &Main;

/* A task that "sleeps" and sends out a message */
void TaskOne (void)
{
	uint8_t delay;

	for (;;) {
	     	printf("TaskOne: Hello World!\n");

		delay = rand() / (RAND_MAX / DELAY_MAX_MS + 1);

		/* BEWARE of the __delay() stuff in preemptive systems. It
		 * measure time by counting CPU cycles. This only works
		 * if the task is not interrupted. Here we do not care
		 * about he actual time spent but just want to give the
		 * task something to do */
		_delay_ms(delay);
	}
}

/* Another task that "sleeps" and sends out a message */
void TaskTwo (void)
{
	uint8_t delay;

	for (;;) {
		printf("TaskTwo: Hello World!\n");

		delay = rand() / (RAND_MAX / DELAY_MAX_MS + 1);

		_delay_ms(delay);
	}
}

/* Randomly select a new task as the "CurrentTask". */
void OS_Scheduler (void)
{
	uint8_t NextRandomTask = rand() / (RAND_MAX / NR_TASKS + 1);
	CurrentTask = &Tasks[NextRandomTask];
}

/* Timer 1 overflow interrupt. Drives the scheduler */
ISR(TIMER1_OVF_vect, ISR_NAKED)
{
	UC_SaveContext();

	OS_Scheduler();

	UC_RestoreContext();

	/* Enable interrupts and return into the selected task */
	asm volatile ( "reti" );
}

/* Helper function for directing the stdout file descriptor to the UART
 * this enables using library functions like PRINTF(3) */
static int SendChar(char c, FILE *stream)
{
        (void)stream;

	/* Concurrent access to the UART is NOT GUARDED ON PURPOSE, just to
	 * see the output being sometimes screwed up by preempting tasks
	 */

        /* Wait for empty transmit buffer */
        while (bit_is_clear(UCSR0A, UDRE0))
                ;
        /* Put tx data into the buffer. It will then be send by the hardware */
        UDR0 = c;

        return 0;
}

int main (int argc, char **argv)
{
	/* Direct stdout and stderr to the microcontroller's UART port.
	 * The port is redirected by the AVR simulator */
        FILE UartDebug = FDEV_SETUP_STREAM(SendChar, NULL, _FDEV_SETUP_WRITE);
        stdout = stderr = &UartDebug;

	/* Pacify the compiler about unused parameters */
	(void)argc;
	(void)argv;

	/* Usefull when unexpected "resets" happen */
        printf("SYSTEM STARTUP\n");

	/* Play with the initialization value to find issues */
	memset(StackTaskOne, 0x00, TASK_STACK_SIZE);
	memset(StackTaskTwo, 0x00, TASK_STACK_SIZE);

        /* Address of the code where the tasks enter */
        StackTaskOne[TASK_STACK_SIZE - 1] = (uint8_t)((uint16_t)TaskOne);
        StackTaskOne[TASK_STACK_SIZE - 2] = (uint8_t)((uint16_t)TaskOne >> 8);
        StackTaskTwo[TASK_STACK_SIZE - 1] = (uint8_t)((uint16_t)TaskTwo);
        StackTaskTwo[TASK_STACK_SIZE - 2] = (uint8_t)((uint16_t)TaskTwo >> 8);

	/* Set up timer1 of the microcontroller */
        TCCR1A = 0x0;
        TCCR1B = 0x1;		/*  1:1 (NO) prescaler */
        TCCR1C = 0x0;
        TIMSK1 = _BV(TOIE1);	/* Enable TC1.ovf interrupt */
        TCNT1H = 0;
        TCNT1L = 0;

	/* Enable interrupts */
	sei();

	/* Wait here until the scheduler starts the first task */
	for (;;) {
	}

	/* Will never be reached. Make the compiler happy by
	 * returning a value to match the function definition */
	return 0;
}

