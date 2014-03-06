/* Copyright (c) 2014 Matt Evans
 *
 * input:  Poll for button presses, and manage click/hold events.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <inttypes.h>
#include "input.h"

#ifdef SIM
#include "sim_disp.h"
#else
#include <stm32f0xx.h>
#include "hw.h"
#include "time.h"
#endif

// Debounce/hold thresholds:
#define DB_THRESH	5
#define DB_HOLD_TIME	100	// 1 second


// Simple debounced buttons:
uint8_t	input_get_raw(void)
{
#ifdef SIM
	return 0;
#else
	int i = ~GPIOA->IDR & 0xb;
	return i;
#endif
}

static	uint8_t 	ev_gen = 0;
static	InputEvent	event = IE_NONE;

InputEvent	input_get_event(void)
{
	InputEvent e = IE_NONE;
	// Events are one-shot; once consumed, return IE_NONE.

#ifdef SIM
	int i = sim_disp_event();
	switch (i & 7) {
	case 1:
		event = (i & 0x80) ? IE_BHOLD : IE_BUP;
		break;
	case 2:
		event = (i & 0x80) ? IE_LHOLD : IE_LUP;
		break;
	case 4:
		event = (i & 0x80) ? IE_RHOLD : IE_RUP;
		break;
	}
#endif
	// BTW: This is racy and not good pracice; one ought to save IRQ mask &
	// then disable IRQs across this.  But, it really doesn't matter much
	// given it's a missed button press and this isn't firmware for a
	// life-support device.  In fact, this comment is longer than writing
	// the correct code, but hey-ho.
	e = event;
	event = IE_NONE;
	return e;
}

#ifndef SIM
static time_callback_t tcb;

static int	dtime_button[3] = {0};

static const int button_bits[] = { 0, 1, 3 };
static const InputEvent button_devts[] = { IE_BDOWN, IE_LDOWN, IE_RDOWN };
static const InputEvent button_uevts[] = { IE_BUP, IE_LUP, IE_RUP };
static const InputEvent button_hevts[] = { IE_BHOLD, IE_LHOLD, IE_RHOLD };
static const InputEvent button_huevts[] = { IE_BHOLDUP, IE_LHOLDUP, IE_RHOLDUP };

static void 	button_samp_tcb(uint64_t t_now)
{
	int i;

	for (i = 0; i < 3; i++) {
		if (!(GPIOA->IDR & (1 << button_bits[i]))) {
			// Pushed
			int t = ++dtime_button[i];
			if (t == DB_HOLD_TIME) {
				event = button_hevts[i];
			} else if (t == DB_THRESH) {
				event = button_devts[i];
			}
		} else {
			// Unpushed
			int t = dtime_button[i];
			if (t > DB_HOLD_TIME) {
				event = button_huevts[i];
			} else if (t > DB_THRESH) {
				event = button_uevts[i];
			}
			dtime_button[i] = 0;
		}
	}
}

void	input_init(void)
{
	// Buttons pull down GPIO to Gnd:
	// middle	PA0
	// left		PA1
	// right	PA3

	// Input, pull-up:
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
	GPIOA->MODER &= ~0xcf;	// 0, 1, 3 input
	GPIOA->PUPDR &= ~0xcf;
	GPIOA->PUPDR |= 0x45;	// Pull-up

	tcb.callback = button_samp_tcb;
	tcb.period = 10; 	/* ms */
        time_callback_periodic(&tcb);
}
#else
void	input_init(void)
{
	// Sim version
}
#endif
