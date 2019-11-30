/*
 * ToRT - Toy-grade OSEK/VXD-inspired RTOS
 *
 * uc.c: Microcontroller stuff
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

#include <avr/io.h>
#include <avr/interrupt.h>

#include "uc.h"
#include "os.h"
#include "ap.h"

#define BUTTON_ROTATE PD2
#define BUTTON_DROP   PD3

#define LED_GREEN     PB0
#define LED_RED	      PB1
#define LED_BACKLIGHT PB2

/* ADC current and last value */
static volatile uint8_t CurentADCValue = 0;

/* Key press detect */
static volatile uint8_t KeyPress;

/* Check if one or more keys are pressed */
static uint8_t KeyPressed(uint8_t Key)
{
	Key &= KeyPress;              /* read key(s) */
	KeyPress ^= Key;              /* clear key(s) */

	return Key;
}

/* All ISRs are sexy (defined naked) i.e. the compiler does not
 * generate any context saving code. The ISR is responsible for
 * managing that */

/* ADC interrupt */
ISR(ADC_vect, ISR_NAKED)
{
	static uint8_t LastValue = 0;

	/* Save the context of the current task */
	Uc_SaveContext();

	/* Read result of analog to digital conversion */
	CurentADCValue = ADCH;

	/* Do not react to any minuscule change in ADC value
	 * but only to sufficently large value changes */
	if ( (CurentADCValue > (LastValue + 10))
	   ||
	     (CurentADCValue < (LastValue - 10))
	  ) {
		uint8_t event = 0;

		/* Detect motion according to the direction in
		 * which the potentiometer was turned */
		if (CurentADCValue < LastValue)
			event = EVENT_RIGHT;
		else
			event = EVENT_LEFT;

		LastValue = CurentADCValue;

		/* Report the event to the control task */
		Os_SetEvent(TASK_ID_CTRL, event);
	}

	/* Restore the context of the newly selected task */
        Uc_RestoreContext();

	/* Enable interrupts and return */
	asm volatile("reti");
}

/* This is the Timer1 overflow ISR that is driving the OS scheduler.
 * It is abused to report key presses */
ISR(TIMER1_OVF_vect, ISR_NAKED)
{
	/* Save the context of the current task */
	Uc_SaveContext();

	/* Reset the timer */
	/* Another interrupt will occur after 50ms due to timer overflow */
	TCNT1H = 0x3C;
	TCNT1L = 0xB0;

	/* Select a new task as the "CurrentTask" */
	Os_Scheduler();

	/* 'rotate' button */
	if (KeyPressed(_BV(BUTTON_ROTATE))) {
		/* Inform the controller task about the event */
		Os_SetEvent(TASK_ID_CTRL, EVENT_ROTATE);
	}

	/* 'drop' button */
	if (KeyPressed(_BV(BUTTON_DROP))) {
		/* Inform the controller task about the event */
		Os_SetEvent(TASK_ID_CTRL, EVENT_DROP);
	}

	/* Restore the context of the task selected by the scheduler */
        Uc_RestoreContext();

	/* Enable interrupts and return into the new (or old) task */
	asm volatile("reti");
}

/* This is the Timer2 overflow ISR that is driving the application
 * timers.
 * It is abused for debouncing the push buttons */
ISR(TIMER2_OVF_vect, ISR_NAKED)
{
	/* Debounced and inverted key state: bit = 1: key pressed */
	static uint8_t KeyState;
	/* 8 * 2bit counters */
	static uint8_t Counter0 = 0xFF;
	static uint8_t Counter1 = 0xFF;
	uint8_t In;

	/* Save the context of the current task */
	Uc_SaveContext();

	/* Tick the (software) application timer(s) */
	Os_TickTimer(TIMER_ID_GAME);

	/* This snippet was ripped from Peter Dannegger's debounce routines.
	 * It counts the number of times the buttons are detected
	 * as pressed within a given period of time to filter out mechanical
	 * bounces */

	In = ~PIND;                             /* read keys (low active) */
	In ^= KeyState;                         /* key changed ? */
	Counter0 = ~( Counter0 & In );       	/* reset or count Counter0 */
	Counter1 = Counter0 ^ (Counter1 & In);  /* reset or count Counter1 */
	In &= Counter0 & Counter1;              /* count until roll over ? */
	KeyState ^= In;                         /* then toggle debounced state */
	KeyPress |= KeyState & In;              /* 0->1: key press detect */

        Uc_RestoreContext();

	/* Enable interrupts and return */
	asm volatile("reti");
}

/* Send a byte over the UART */
void Uc_UARTSend(uint8_t Data)
{
	/* Wait for empty transmit buffer */
        while (bit_is_clear(UCSR0A, UDRE0))
                ;
	/* Put tx data into the buffer. It will then be send by the hardware */
	UDR0 = Data;
}

/* Return the last read value of the ADC */
inline uint8_t Uc_ADCGet(void)
{
	return CurentADCValue;
}

/* Set the timer driving the scheduler to expire within a tick
 * This will cause the scheduler to run */
inline void Uc_ForceSchedule(void)
{
	TCNT1H = 0xFF;
	TCNT1L = 0xFF;
}

/* Turn the green led on */
inline void Uc_LEDGreenOn(void)
{
	PORTB |= _BV(LED_GREEN);
}

/* Turn the green led off */
inline void Uc_LEDGreenOff(void)
{
	PORTB &= ~_BV(LED_GREEN);
}

/* Turn the red led on */
inline void Uc_LEDRedOn(void)
{
	PORTB |= _BV(LED_RED);
}

/* Turn the red led off */
inline void Uc_LEDRedOff(void)
{
	PORTB &= ~_BV(LED_RED);
}

/* Turn on the LCD's backlight LEDs */
inline void Uc_LCDBacklightOn(void)
{
	PORTB |= _BV(LED_BACKLIGHT);
}

/* Turn the LCD backlight led off */
inline void Uc_LCDBacklightOff(void)
{
	PORTB &= ~_BV(LED_BACKLIGHT);
}

/* Initialize all the peripheral components of the microcontroller
 * that are used by this project */
uint8_t Uc_HardwareInit(void)
{
	/* Initialize Ports */
	DDRB  = 0x07;		/* Port B 0..2 -> output */
	PORTB = 0xF8;		/* pulls-ups for inputs */

        DDRC  = 0x37;		/* Port C 6..0 -> output C3 input -> adc */
	PORTC = 0xC0;		/* enable pull-ups for uppermost 2 bits */

	DDRD  = 0x02;		/* Port D - all inputs except UART TX - pin 1*/
	PORTD = 0xFC;		/* Enable internal pull-ups for all but RX and TX */

	/* Initialize Timer1 */
	TCCR1A = 0x00;
	TCCR1B = 0x02;		/* 8 prescaler. Low latency is important for forcing a task switch
       				 * which is done by advancing the timer	*/
	TCCR1C = 0x00;
	TIMSK1 = 0x01;		/* Enable TC1.ovf interrupt */
	TCNT1H = 0x3C;		/* Interrupt after 50ms due to timer overflow */
	TCNT1L = 0xB0;

	/* Initialize Timer2 */
	TCCR2A = 0x00;
	TCCR2B = 0x05;		/* 32 prescaler */
	TIMSK2 = 0x01;
	TCNT2  = 0x00; 	  	/* Interrupt every 0.004096 seconds  due to overflow) */

	/* Initialize UART
         * Set format N81, enable RX, TX and RX interrupt */
        UCSR0B = 0x00;
        UBRR0 = (uint8_t)((F_CPU / UART_BAUDRATE + 8) / 16 - 1);
        UCSR0B = 0x98;

	/* Initialize ADC */
	ADMUX = 0x63; 		/* AVCC with external capacitor at AREF pin, align left, chanel 3 */

	/* enable adc, auto trigger, enable interrupt, prescaler = 128 */
	ADCSRA = 0xEF;

	/* ADC TIMER1 overflow mode. Free running mode would mean to be interrupted every 200us */
	ADCSRB = 0x06;
	ACSR   = 0x80;		/* Disable analog comp */

	/* Application specific initializations */

        /* Enable the LCD display (SCE -> LOW) */
        PORTC &= 0xF3;

        /* Reset the display RST LOW -> HIGH transition*/
        PORTC &= 0xFD;
        PORTC |= 0x02;

	return 0;
}

