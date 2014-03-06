/* Copyright (c) 2014 Matt Evans
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

#ifndef RTC_H
#define RTC_H

#include <inttypes.h>

void rtc_init(void);

// Was softboot

typedef struct {
	uint8_t 	hour;	// 0-11
	uint8_t 	min;	// 0-59
	uint8_t		sec;	// 0-59
	uint8_t		subsec;	// 0-63
	uint8_t		amnpm;
} tod_t;

void rtc_gettime(tod_t *time_out);
void rtc_settime(tod_t *time);

// Misc:  Get whether cold/warm restart?

#endif
