/* Copyright (c) 2014 Matt Evans
 *
 * main file for Colourclock firmware.  Manage input events, basic UI.
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

#include "types.h"
#include "rtc.h"
#include "display_effects.h"
#include "input.h"
#include "time.h"
#include "lightsense.h"

#ifdef SIM
#include <stdio.h>
#include "sim_disp.h"
#endif

#ifndef SIM
#include "hw.h"
#include "led_disp.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// Data

enum { ST_NORMAL, ST_SET_TIME_H, ST_SET_TIME_M, ST_SET_BRI_MAX, ST_SET_BRI_MIN };

static int state = ST_NORMAL;
static int st_cur;

////////////////////////////////////////////////////////////////////////////////

static void update_display(unsigned int framenum)
{
	pix_t fb_data[60];

	switch (state) {
	case ST_NORMAL: {
		tod_t time;
		rtc_gettime(&time);
		display_draw(fb_data, framenum, &time);
	} break;
	case ST_SET_TIME_H: {
		uint32_t t = (uint32_t)time_getglobal();
		display_drawspecial(fb_data, DS_HR, t & 0x380,
				    st_cur);
	} break;
	case ST_SET_TIME_M: {
		uint32_t t = (uint32_t)time_getglobal();
		display_drawspecial(fb_data, DS_MIN, t & 0x380,
				    st_cur);
	} break;
	case ST_SET_BRI_MAX:
	case ST_SET_BRI_MIN: {
		uint32_t t = (uint32_t)time_getglobal();
		display_drawspecial(fb_data,
				    (state == ST_SET_BRI_MAX) ? DS_BR_H : DS_BR_L,
				    t & 0x380,
				    st_cur);
	} break;
	}

#ifdef SIM
	sim_disp_sync(fb_data);
#else
	led_fb_to_pwm_buffer(fb_data, 1);
#endif
}

static void process_input(void)
{
	// display_next/display_prev
	int i;
	static tod_t time;

	if (state == ST_NORMAL) {
		switch (input_get_event()) {
		case IE_RUP:
			display_next();
			break;
		case IE_LUP:
			display_prev();
			break;
		case IE_BHOLD: {
			rtc_gettime(&time);
			st_cur = time.hour;
			state = ST_SET_TIME_H;
		} break;
		default:
			break;
		}
	} else if (state == ST_SET_TIME_H) {
		switch (input_get_event()) {
			// This is very basic; a 'hold' would be nice
			// for acceleration or a fast-tick mode.
		case IE_RUP:
			if (++st_cur > 11)
				st_cur = 0;
			break;
		case IE_LUP:
			if (--st_cur < 0)
				st_cur = 11;
			break;
		case IE_BUP: {
			time.hour = st_cur;
			st_cur = time.min;
			state = ST_SET_TIME_M;
		} break;
		case IE_BHOLD: {
			state = ST_SET_BRI_MAX;
			st_cur = lightsense_get_max();
		} break;

		default:
			break;
		}
	} else if (state == ST_SET_TIME_M) {
		switch (input_get_event()) {
		case IE_RUP:
			if (++st_cur > 59)
				st_cur = 0;
			break;
		case IE_LUP:
			if (--st_cur < 0)
				st_cur = 59;
			break;
		case IE_BUP: {
			time.min = st_cur;
			time.sec = 0;
			time.subsec = 0;
			rtc_settime(&time);
			state = ST_NORMAL;
		} break;
		default:
			break;
		}
	} else if (state == ST_SET_BRI_MAX) {
		switch (input_get_event()) {
		case IE_RUP:
			st_cur += 8;
			if (st_cur > 255)
				st_cur = 255;
			lightsense_set_max(st_cur);
			break;
		case IE_LUP:
			st_cur -= 8;
			if (st_cur < 0)
				st_cur = 0;
			lightsense_set_max(st_cur);
			break;
		case IE_BUP: {
			state = ST_SET_BRI_MIN;
			st_cur = lightsense_get_min();
		} break;
		case IE_BHOLD: {
			lightsense_save_limits();
			state = ST_NORMAL;
		} break;
		default:
			break;
		}
	} else if (state == ST_SET_BRI_MIN) {
		switch (input_get_event()) {
		case IE_RUP:
			st_cur += 8;
			if (st_cur > 255)
				st_cur = 255;
			lightsense_set_min(st_cur);
			break;
		case IE_LUP:
			st_cur -= 8;
			if (st_cur < 0)
				st_cur = 0;
			lightsense_set_min(st_cur);
			break;
		case IE_BUP: {
			state = ST_SET_BRI_MAX;
			st_cur = lightsense_get_max();
		} break;
		case IE_BHOLD: {
			lightsense_save_limits();
			state = ST_NORMAL;
		} break;
		default:
			break;
		}
	}
}

static void wait_vsync(void)
{
#ifndef SIM
	// Wait for vsync; WFI and check flag
	// Double-buffer, as we now update new buffer whilst
	// old one is being drawn.
	led_fb_vsync_swap();
#endif
}

int main(
#ifdef SIM
	int argc, char *argv[]
#else
	void
#endif
	)
{
	unsigned int frames = 0;

#ifndef SIM
	// HW clocks/PLL init
	hw_init();
	// Also, init timers/debug/input before the rest.
#endif
	rtc_init();
	// Display uses rtc (to get the saved state); init last:
	display_init();

#ifdef SIM
	sim_disp_init(argc, argv);
#else
	led_disp_init();

//	Nein!
//	led_test();
#endif

	while(1) {
		process_input();
		update_display(frames++);
		wait_vsync();

		// Misc debug callbacks here
	}

	return 0;
}
