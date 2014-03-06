/* Copyright (c) 2014 Matt Evans
 *
 * uart:  Drive STM32 UART for debug.  Includes printf implementation.
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

#include <stdarg.h>
#include <stddef.h>
#include <inttypes.h>
#include <stm32f0xx.h>

#include "uart.h"
#include "hw.h"

#define UART1_BAUD 19200


void uart_putch(char c)
{
	while ((USART1->ISR & USART_ISR_TXE) == 0) {}

	USART1->TDR = c;
}

char uart_getch(void)
{
	return USART1->RDR;
}

int uart_ch_rdy(void)
{
	return !!(USART1->ISR & USART_ISR_RXNE);
}

void uart_init(void)
{
	// PA9 PA10 USART1 = AF1
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
	RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

        GPIOA->AFR[1] = (GPIOA->AFR[1] & ~0xff0) | 0x110; // AF1
	/* No pullup/down */
	GPIOA->PUPDR = (GPIOA->PUPDR & ~0x3c0000);
	/* A9/A10 alternate function */
	GPIOA->MODER = (GPIOA->MODER & ~0x3c0000) | 0x280000;

	USART1->CR1 = 0; /* 8N */
	USART1->BRR = SYS_CLK/UART1_BAUD;
	USART1->CR2 = 0; /* 1 stop */
	USART1->CR1 |= USART_CR1_UE;
	USART1->CR1 |= USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

static void pdec(void (*putch)(char, void*), void *putarg,
		 unsigned long int n, int unsgnd)
{
	char buff[21]; /* 2^64 is 20 digits */
	char *p = buff;

	/* Write to buffer from lowest digit first, then read out in reverse */
	if (!unsgnd) {
		long ns = n;
		if (ns < 0) {
			putch('-', putarg);
			n = -ns;
		}
	}

	do { 
		*p++ = '0' + (n % 10);
	} while ((n /= 10) != 0);

	while (p > buff)
	{
		putch(*--p, putarg);
	} 
}

static void phex(void (*putch)(char, void*), void *putarg,
		 unsigned long int n, int digits, int caps, char pad)
{
	int i;
	char b = caps ? 'A' : 'a';
	for (i = digits-1; i >= 0; i--) {
		int d = (n >> (i*4)) & 0xf;
		if ((d != 0) || (i == 0)) {
			if (d > 9)
				putch(b + d - 10, putarg);
			else
				putch('0' + d, putarg);
		} else {
			if (pad != 0)
				putch(pad, putarg);
		}
	}
}

/* Simple dumb printf-style scanner. */
static void do_printf_scan(void (*putch)(char, void*), void *putarg,
			   const char *fmt, va_list args)
{
	char *cp;
	char c;

	while ((c = *fmt++) != '\0') {
		int saw_long = 0;
		int saw_unsigned = 0;
		int saw_zeropad = 0;
		int saw_spacepad = 0;
		int pr_digits = 0;

		if (c != '%') {
			putch(c, putarg);
			continue;
		}
		/* Else ... Hit a %, what's the next char? */
	parse_perc:
		if ((c = *fmt++) != '\0') {
			switch (c) {
				case '%':
					putch('%', putarg);
					break;
				case 'l':
					saw_long = 1;
					goto parse_perc;
					break;
				case 'u':
					saw_unsigned = 1;
					goto parse_perc;
					break;
				case 's':
					cp = va_arg(args, char *);
					while ((c = (*cp++)) != 0) {
						putch(c, putarg);
					}
					break;
				case 'p':
					putch('0', putarg);
					putch('x', putarg);
					saw_long = 1;
				case 'x':
				case 'X':
				{
					char p = 0;
					if (saw_spacepad && pr_digits != 0)
						p = ' ';
					else if (saw_zeropad)
						p = '0';

					if (saw_long)
						phex(putch, putarg, va_arg(args, long),
						     (pr_digits == 0) ? 16 : pr_digits, (c == 'X'), p);
					else
						phex(putch, putarg, va_arg(args, int),
						     (pr_digits == 0) ? 8 : pr_digits, (c == 'X'), p);
				}
				break;
				case 'c':
					putch(va_arg(args, int), putarg);
					break;
					/* Numbers -- zero sets zeropad, others set digits */
				case '0':
					if (!saw_zeropad) {
						saw_zeropad = 1;
						goto parse_perc;
					} /* Else fall through */
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					pr_digits = (pr_digits * 10) + (c - '0');
					goto parse_perc;
				case ' ':
					saw_spacepad = 1;
					goto parse_perc;
					break;
				case 'd':
					if (saw_long)
						pdec(putch, putarg, va_arg(args, long), saw_unsigned);
					else
						pdec(putch, putarg, va_arg(args, int), saw_unsigned);
					break;
					/* Things unsupported just yet... */
				case 'o':
					putch('[', putarg);
					phex(putch, putarg, va_arg(args, int), 8, 0, 0);
					putch(']', putarg);
					break;
				default:
					putch(c, putarg);
			}
		}
	}
}


static void u0_putch(char c, void *arg)
{
	/* Write to UART */
	uart_putch(c);
}

void printf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	do_printf_scan(u0_putch, NULL, fmt, args);
	va_end(args);
}
