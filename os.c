/*
 * ToRT - Toy-grade OSEK/VXD-inspired RTOS
 *
 * os.c: Operating system
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

#include <stdio.h>
#include <avr/sleep.h>

#include "os.h"

/* The stack pointer of the main context is written to this before a switch
 * into the first OS task occurs */
static uint8_t MainContextSP[2];

/* The main context descriptor. Only cares about the stack pointer */
static TaskDescriptor Main = {&MainContextSP[0], 0, 0, 0, 0, 0};

/* Saving the task context will write the old main stack pointer
 * to the above buffer. NULL would mean killing stuff where the
 * reset vector points to, if possible (ROM vs RAM) */
TaskDescriptor volatile *CurrentTask = &Main;

/* Index into the "scheduling table". Indicates the descriptor of the currently
 * running task */
static volatile uint8_t CurrentTaskIndex = 0;

/* Keeps track of which resources are currently occupied. */
static volatile uint8_t ResourcesOccupied = 0;

/* Timers configured by the application */
static TimerDescriptor *Timers = NULL;

/* Tasks configured by the application */
static TaskDescriptor *Tasks = NULL;
static uint8_t NrTasks = 0;

/*
 * Select the task with the highest priority level and which has no blocked
 * resources from all the tasks that are in state READY.
 * If the selected task has a higher priority than the currently running task,
 * or if the current task has transitioned to WAITING or is not yet running
 * (system startup), preempt the currently running task and continue executing
 * the new task, otherwise continue with the currently running task.
 */
void Os_Scheduler(void)
{
	uint8_t TaskIndex = 0;
	uint8_t NextTaskIndex = 0;
	uint8_t HighestPriority = 0;

	/* Look for a suitable task to run next.
	 * Criteria: State, Priority, Resources */
	for (TaskIndex = 0; TaskIndex < NrTasks; TaskIndex++) {
		/* Task READY and has no resources blocked? */
		if ( (Tasks[TaskIndex].State == TASK_STATE_READY)
		    &&
		     ((Tasks[TaskIndex].RequiredResources & ResourcesOccupied) == 0)
		   ) {
			if (Tasks[TaskIndex].Priority > HighestPriority) {
				HighestPriority = Tasks[TaskIndex].Priority;
				NextTaskIndex = TaskIndex;
			}
		}
	}

	/* If the current task moved into a different state than RUNNING
	 * or if it is RUNNING but a task with higher priority has been
	 * selected to run next, do preempt the current task */
	if ( (Tasks[CurrentTaskIndex].State == TASK_STATE_READY)
	    ||
	     (Tasks[CurrentTaskIndex].State == TASK_STATE_WAITING)
	   ) {
		Tasks[NextTaskIndex].State = TASK_STATE_RUNNING;
		CurrentTaskIndex = NextTaskIndex;
		CurrentTask = &Tasks[NextTaskIndex];
	}
	else if (Tasks[CurrentTaskIndex].State == TASK_STATE_RUNNING) {
		if (Tasks[NextTaskIndex].Priority > Tasks[CurrentTaskIndex].Priority) {
			Tasks[CurrentTaskIndex].State = TASK_STATE_READY;
			Tasks[NextTaskIndex].State = TASK_STATE_RUNNING;

			CurrentTaskIndex = NextTaskIndex;
			CurrentTask = &Tasks[NextTaskIndex];

		}
		/* else:  No preemption, continue with currently running task */
	}
}

/* Decrement the application timers. */
void Os_TickTimer(uint8_t TimerID)
{
	Os_EnterCritical();

	/* Ignore inactive timers */
	if (Timers[TimerID].Value > 0) {

		Timers[TimerID].Value--;

		/* When the timer reaches 0 the configured event is sent to the
		 * task owning the timer */
		if (Timers[TimerID].Value == 0)
			Os_SetEvent(Timers[TimerID].TaskID, Timers[TimerID].Event);
	}

	Os_ExitCritical();
}

/*
 * This call serves to enter a critical section in the code that is assigned to
 * the resource referenced by ResID.
 * A critical section shall always be left using ReleaseResource within the
 * same task (function). While holding a resource the task may not enter waiting
 * state i.e. wait for any event.
 *
 * To prevent priority inversion something similar to the priority ceiling
 * protocol is used: All tasks must declare interrest in a resource in their
 * task descriptor at system configuration time. While the resource is held by
 * any of the interrested tasks another taks that requires that resource will
 * not be scheduled, even if it is READY and of higher priority than the
 * currently running task.
 *
 * Nested resource occupation is only allowed if the inner critical sections are
 * completely executed within the surrounding critical section - strictly stacked.
 */
inline void Os_GetResources(uint8_t ResID)
{
	Os_EnterCritical();

	/* Occupy the resource */
	ResourcesOccupied |= ResID;

	Os_ExitCritical();
}

/*
 * ReleaseResource is the counterpart of GetResource and serves to leave critical
 * sections in the code that are assigned to the referenced resource.
 */
void Os_ReleaseResources(uint8_t ResID)
{
	Os_EnterCritical();

	/* Release the resource */
	ResourcesOccupied &= ~ResID;

	/* This task freed events that might be required by another
	 * high priority task in order to run */

	/* FIXME: find out if there are any */
	Os_ForceSchedule();

	Os_ExitCritical();
}

/*
 * The event is set as given by the mask.
 * Calling SetEvent causes the targeted task to be transferred to the READY state,
 * if it was waiting for the event specified in mask. If it is of higher
 * priority than the currently running task the scheduler is set to run ASAP
 * so a timely switch to higher prio task can occur
 */
void Os_SetEvent(uint8_t TaskID, uint8_t Mask)
{
	Os_EnterCritical();

	/* Set the event in the descriptor of the current task */
	Tasks[TaskID].Events |= Mask;

	/* If the task was waiting for this event move it to the READY state */
	if (Tasks[TaskID].WaitForEvents & Tasks[TaskID].Events) {

		Tasks[TaskID].State = TASK_STATE_READY;

		/* If the priority of the addressed task is higher than the
		 * current task trigger scheduling ASAP */
		if (Tasks[TaskID].Priority > Tasks[CurrentTaskIndex].Priority)
			Os_ForceSchedule();
	}

	Os_ExitCritical();
}

/*
 * Clear the events specified in the mask from the current tasks's events.
 * ClearEvents is restricted to the task which owns the events (CurrentTask).
 */
inline void Os_ClearEvents(uint8_t Mask)
{
	Os_EnterCritical();

	Tasks[CurrentTaskIndex].Events &= ~Mask;

	Os_ExitCritical();
}

/*
 * This service returns the current state of all event bits of the currently
 * active task.
 */
inline uint8_t Os_GetEvents(void)
{
	uint8_t events;

	Os_EnterCritical();

	events = Tasks[CurrentTaskIndex].Events;

	Os_ExitCritical();

	return events;
}

/*
 * The state of the current task is set to WAITING, unless at least
 * one of the events specified in Mask have already been set.
 * This call enforces scheduling, if the wait condition occurs.
 */
void Os_WaitEvents(uint8_t Mask)
{
	Os_EnterCritical();

	/* Mark the events waited on in the task descriptor */
	Tasks[CurrentTaskIndex].WaitForEvents |= Mask;

	/* If none of the requested events is already set, wait for one */
	if ((Tasks[CurrentTaskIndex].Events & Mask) == 0) {

		Tasks[CurrentTaskIndex].State = TASK_STATE_WAITING;

		/* This task goes into WAITING so trigger a scheduling
		 * before to avoid wasting CPU cycles and reaction time
		 */
		Os_ForceSchedule();

		Os_ExitCritical();

		/* Task goes to WAITING state until it gets one of the
		 * events it cares for. It does not really do the busy-waiting
		 * the for(;;) indicates, but just stays there for a few iterations
		 * until the scheduler runs. After that it is scheduled again.
		 * It will exit the loop immediately due to the set event - when
		 * it occurs
		 */
		for (;;) {
			if ((Tasks[CurrentTaskIndex].Events & Mask))
				break;
		}
	}
	else {
		Os_ExitCritical();
	}
}

/*
 * Start a given timer by setting its value
 */
inline void Os_SetTimer(uint8_t TimerID, uint8_t Value)
{
	Os_EnterCritical();

	Timers[TimerID].Value = Value;

	Os_ExitCritical();
}

/*
 * Set up the relevant OS tables and start the operating system.
 * This call does not return.
*/
void Os_StartOS(TaskDescriptor *ApTasks, uint8_t NrApTasks, TimerDescriptor *ApTimers)
{
	/* Scheduling table stuff */
	Tasks = ApTasks;
	NrTasks = NrApTasks;

	/* Timers */
	Timers = ApTimers;

	/* Enable the interrupts */
	Os_EnableAllInterrupts();

        /* Do nothing. The Scheduler will run with the next Timer1 ISR */
        for (;;) {
		/* Sleep until an interrupt happens */
		set_sleep_mode(SLEEP_MODE_IDLE);
		sleep_mode();
        }
}

/*
 * Call this system service to abort the system (e.g. emergency off).
 */
void Os_ShutdownOS(void)
{
	/* Turn off all interrupts */
	Os_DisableAllInterrupts();

	for (;;) {
		/* Sleep forever */
		sleep_cpu();
	}
}

