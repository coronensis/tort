/*
 * ToRT - Toy-grade OSEK/VXD-inspired RTOS
 *
 * uc.h: Microcontroller stuff
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

#ifndef UC_H
#define UC_H

/* Speed at which the UART communicates in bits/sec (baud) */
#define UART_BAUDRATE		57600

/* Number of bytes required to save a context.
 * 32 * 1 byte registers + 1 status byte + 2 bytes stack pointer */
#define SIZE_SAVED_CONTEXT	35

/* Absolute stack size minimum a task can use */
#define TASK_STACK_SIZE_MIN	SIZE_SAVED_CONTEXT

/* Save the context of CurrentTask */
#define Uc_SaveContext() \
        asm volatile("push r0");\
        asm volatile("in   r0, __SREG__");\
        asm volatile("cli");\
        asm volatile("push r0");\
        asm volatile("push r1");\
        asm volatile("clr  r1");\
        asm volatile("push r2");\
        asm volatile("push r3");\
        asm volatile("push r4");\
        asm volatile("push r5");\
        asm volatile("push r6");\
        asm volatile("push r7");\
        asm volatile("push r8");\
        asm volatile("push r9");\
        asm volatile("push r10");\
        asm volatile("push r11");\
        asm volatile("push r12");\
        asm volatile("push r13");\
        asm volatile("push r14");\
        asm volatile("push r15");\
        asm volatile("push r16");\
        asm volatile("push r17");\
        asm volatile("push r18");\
        asm volatile("push r19");\
        asm volatile("push r20");\
        asm volatile("push r21");\
        asm volatile("push r22");\
        asm volatile("push r23");\
        asm volatile("push r24");\
        asm volatile("push r25");\
        asm volatile("push r26");\
        asm volatile("push r27");\
        asm volatile("push r28");\
        asm volatile("push r29");\
        asm volatile("push r30");\
        asm volatile("push r31");\
        asm volatile("lds  r26, CurrentTask");\
        asm volatile("lds  r27, CurrentTask + 1");\
        asm volatile("in   r0, __SP_L__");\
        asm volatile("st   x+, r0");\
        asm volatile("in   r0, __SP_H__");\
        asm volatile("st   x+, r0");

/* Restore the context of CurrentTask */
#define Uc_RestoreContext() \
        asm volatile("lds r26, CurrentTask");\
        asm volatile("lds r27, CurrentTask + 1");\
        asm volatile("ld  r28, x+");\
        asm volatile("out __SP_L__, r28");\
        asm volatile("ld  r29, x+");\
        asm volatile("out __SP_H__, r29");\
        asm volatile("pop r31");\
        asm volatile("pop r30");\
        asm volatile("pop r29");\
        asm volatile("pop r28");\
        asm volatile("pop r27");\
        asm volatile("pop r26");\
        asm volatile("pop r25");\
        asm volatile("pop r24");\
        asm volatile("pop r23");\
        asm volatile("pop r22");\
        asm volatile("pop r21");\
        asm volatile("pop r20");\
        asm volatile("pop r19");\
        asm volatile("pop r18");\
        asm volatile("pop r17");\
        asm volatile("pop r16");\
        asm volatile("pop r15");\
        asm volatile("pop r14");\
        asm volatile("pop r13");\
        asm volatile("pop r12");\
        asm volatile("pop r11");\
        asm volatile("pop r10");\
        asm volatile("pop r9");\
        asm volatile("pop r8");\
        asm volatile("pop r7");\
        asm volatile("pop r6");\
        asm volatile("pop r5");\
        asm volatile("pop r4");\
        asm volatile("pop r3");\
        asm volatile("pop r2");\
        asm volatile("pop r1");\
        asm volatile("pop r0");\
        asm volatile("out __SREG__, r0");\
        asm volatile("pop r0");

/* Protect against concurrency issues by disabling the interrupts
 * while in a critical section */
#define Uc_EnterCritical() \
        asm volatile("in __tmp_reg__, __SREG__");\
        asm volatile("cli");\
        asm volatile("push  __tmp_reg__");

/* Renable the global interrupt flag (if it was set) upon exiting
 * a critical section */
#define Uc_ExitCritical() \
        asm volatile("pop __tmp_reg__");\
        asm volatile("out __SREG__, __tmp_reg__");

/* Turn off the global interrupt enable flag thus disabling all interrupts */
#define Uc_DisableAllInterrupts() \
        asm volatile("cli");

/* Antagonist of Uc_DisableAllInterrupts() */
#define Uc_EnableAllInterrupts() \
        asm volatile("sei");

/* Initialize the microcontroller */
extern uint8_t Uc_HardwareInit(void);

/* Get the value of the last analog to digital conversion */
extern uint8_t Uc_ADCGet(void);

/* Send a byte over the UART */
extern void Uc_UARTSend(uint8_t Data);

/* Force the operating system scheduler to run ASAP */
extern void Uc_ForceSchedule(void);

/* Control of LEDs */
extern void Uc_LEDGreenOn(void);
extern void Uc_LEDGreenOff(void);
extern void Uc_LEDRedOn(void);
extern void Uc_LEDRedOff(void);
extern void Uc_LCDBacklightOn(void);
extern void Uc_LCDBacklightOff(void);

#endif /* UC_H */

