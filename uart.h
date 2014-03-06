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

#ifndef UART_H
#define UART_H

void uart_putch(char c);
char uart_getch(void);
int uart_ch_rdy(void);
static inline void uart_pstr(char *s)
{
	while (*s)
		uart_putch(*s++);
}

void uart_init(void);

void printf(const char *fmt, ...);

#endif
