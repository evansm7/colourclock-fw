/* Copyright (c) 2014 Matt Evans
 *
 * time:  Simple callback-based periodic timer and delay routines
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

#include <stm32f0xx.h>
#include <stm32f0xx_rcc.h>
#include "time.h"
#include "hw.h"


volatile uint64_t global_time = 0;
time_callback_t *cb_list = 0;


void	delay_ms(int d)
{
	uint64_t tn;

	__disable_irq();
	tn = global_time;
	__enable_irq();

	while ((global_time - tn) < d) {
		__WFI();
	}
}

void	delay_us(int d)
{
	int i;
	for (i = 0; i < (d*48/2); i++) 
	{
		// Ish... prob.
	}
}

void 	SysTick_Handler(void)
{
	static int on = 0;
	time_callback_t *c = cb_list;

	on++;

	global_time++;

	/* Process callbacks */
	while (c) {
		if (c->next_tick <= global_time) {
			c->callback(global_time);
			c->next_tick += c->period;
		}
		c = c->next;
	}
}

void 	time_callback_periodic(time_callback_t *c)
{
	c->next = cb_list;
	cb_list = c;

	__disable_irq();
	c->next_tick = global_time + c->period;
	__enable_irq();
}

void 	time_init(void)
{
	SysTick_Config(48000000/1000);

	/* Set up TIM2 as 32bit upcounting for high-precision tickin' */
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

	TIM2->ARR = 0xffffffff;
	TIM2->CR2 = 0;
	TIM2->SMCR = 0;
	TIM2->DIER = 0;
	TIM2->EGR = 0;
	TIM2->CCMR1 = 0;
	TIM2->CCER = 0;
	TIM2->CNT = 0;
	TIM2->PSC = 0;
	TIM2->CR1 = TIM_CR1_CEN; /* Enabled, upcounting, nothing fancy */
}

uint32_t time_getfine(void)
{
	return TIM2->CNT;
}
