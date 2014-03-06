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

#ifndef TIME_H
#define TIME_H

void	delay_ms(int d);
void	delay_us(int d);

/* Get us? get ms?  global_time's not that accurate */

typedef struct _tcallback {
	void (*callback)(uint64_t t_now);
	uint64_t period;
	uint64_t next_tick;
	struct _tcallback *next;
} time_callback_t;

void	time_init(void);
void	time_callback_periodic(time_callback_t *c);
uint32_t time_getfine(void);

extern volatile uint64_t global_time;
#ifndef SIM
static inline uint64_t time_getglobal(void)
{
	return global_time;
}
#else
uint64_t time_getglobal(void);
#endif

#endif
