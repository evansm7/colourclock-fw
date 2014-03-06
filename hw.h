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

#ifndef hw_H
#define hw_H

#include <stm32f0xx.h>

#define B(x) (1 << (x))
#define B2(x, y) ((x) << ((y)*2))

#define B_SA 11
#define B_SB 12
#define B_SC 13

#define B_LE 14
#define B_OE 15

#define A_SI 7
#define A_CK 5

//#define BUTTON_PRESSED (GPIOA->IDR & 1)

// LDR is on PB0 -- ADC_IN8
#define ADC_LDR	8
#define GPIO_LDR	GPIOB
#define GPIO_LDR_CH	0
#define LDR_GPIO_EN	RCC_AHBENR_GPIOBEN

void	hw_init(void);

#define SYS_CLK 48000000

#endif
