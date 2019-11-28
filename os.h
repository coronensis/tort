/*
 * ToRT - Toy-grade OSEK/VXD-inspired RTOS
 *
 * os.h: Operating system
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

#ifndef OS_H
#define OS_H

#include <stdint.h>
#include "uc.h"

/* Task Management */

/* States in which a task can be */
enum {
     TASK_STATE_READY
    ,TASK_STATE_RUNNING
    ,TASK_STATE_WAITING
};

/* Structure to describe a task */
typedef struct {
	/* Pointer to the stack reserved for this task.
	 * !!! MUST BE THE FIRST ENTRY IN THIS STRUCTURE !!! */
	const void *Stack;

	/* The state in which the task is. */
	volatile uint8_t State;

	/* Currently set events. Bit mask. */
	volatile uint8_t Events;

	/* Events the task is waiting for. Bit mask */
	volatile uint8_t WaitForEvents;

	/* Required resources. Bit mask. */
	const uint8_t RequiredResources;

	/* Configured priority of the task. */
	const uint8_t Priority;
} TaskDescriptor;

/* Task scheduler */
extern void Os_Scheduler(void);

/* Force the scheduler to be invoked ASAP */
#define Os_ForceSchedule	Uc_ForceSchedule

/* Pointer to the descriptor of the currently executing task */
extern TaskDescriptor volatile *CurrentTask;

/* Hardware access */

/* Initialize the microcontroller */
#define Os_HardwareInit		Uc_HardwareInit

/* Access to UART and ADC */
#define Os_UARTSend		Uc_UARTSend
#define Os_ADCGet		Uc_ADCGet

/* Blinky stuff */
#define Os_LEDGreenOn		Uc_LEDGreenOn
#define Os_LEDGreenOff		Uc_LEDGreenOff
#define Os_LEDRedOn		Uc_LEDRedOn
#define Os_LEDRedOff		Uc_LEDRedOff
#define Os_LCDBacklightOn	Uc_LCDBacklightOn
#define Os_LCDBacklightOff	Uc_LCDBacklightOff

/* Interrupt handling */

#define Os_EnableAllInterrupts	Uc_EnableAllInterrupts
#define Os_DisableAllInterrupts	Uc_DisableAllInterrupts

/* Critical sections */
#define Os_EnterCritical	Uc_EnterCritical
#define Os_ExitCritical		Uc_ExitCritical

/* Resource management */

/* Resources that need exclusive access */
#define RESOURCE_NONE			0

/* Occupy one or more resources */
extern void Os_GetResources(uint8_t ResID);

/* Free one or more resources */
extern void Os_ReleaseResources(uint8_t ResID);

/* Event control */

#define EVENT_NONE			0

/* Set an event to a task */
extern void Os_SetEvent(uint8_t TaskID, uint8_t Mask);

/* Clear one or more events of the current task */
extern void Os_ClearEvents(uint8_t Mask);

/* Get the event set to the current task */
extern uint8_t Os_GetEvents(void);

/* Make the current task wait for one or more events */
extern void Os_WaitEvents(uint8_t Mask);

/* Timers */

/* Start a timer by setting its value to something different than  0 */
extern void Os_SetTimer(uint8_t TimerID, uint8_t Value);

/* Decrement a timer. When it reaches zero it will send the configured
 * event to the task it is assigned to */
extern void Os_TickTimer(uint8_t TimerID);

typedef struct {
	/* Nr of ticks to timer expiration */
	volatile uint8_t Value;

	/* Task assigned to the timer */
	const uint8_t TaskID;

	/* Event sent to the assigned task upon expiry */
	const uint8_t Event;
} TimerDescriptor;

/* Duration of a tick of the system timer in miliseconds. */
#define OS_TICK_DURATION		50

/* Duration of a tick of application timers in miliseconds. */
#define APP_TICK_DURATION		4

/* Operating system startup/shutdown control */

/* Initialize and start the OS */
extern void Os_StartOS(TaskDescriptor *ApTasks, uint8_t NrApTasks, TimerDescriptor *ApTimers);

/* Halt the OS */
extern void Os_ShutdownOS(void);

#endif /* OS_H */

