/* Copyright (c) 2014 Matt Evans
 *
 * hw:  Basic hardware init.
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
#include <stm32f0xx.h>
/* Yuck, I am using the STM libs after all.  Just this one... */
#include <stm32f0xx_rcc.h>

#include "hw.h"
#include "time.h"
#include "spi.h"
#include "uart.h"
#include "lightsense.h"
#include "input.h"
#include "flashvars.h"

static void	io_init(void)
{
	// Don't fuck up the SWDBG pins:
        GPIOA->MODER   = (GPIOA->MODER & 0xffffccff) | 0x00004400;
	GPIOA->OTYPER  = 0; // Push-pull
	GPIOA->OSPEEDR = 0x0000cc00; // 50MHz/high current

	GPIOB->MODER   = 0x55400000;
	GPIOB->OTYPER  = 0; // Push-pull
	GPIOB->OSPEEDR = 0xffc00000; // 50MHz/high current
}

static void	setup_hse_clock(void)
{
	/* When using sawn-off board, there's no 8MHz MCO fed in for HSE, so
	 * must use the internal 8MHz RC then PLL that up x 6.
	 *
	 * Set HSI as clock source:
	 */
	/* Enable Prefetch Buffer and set Flash Latency */
	FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY;
	/* HCLK = SYSCLK */
	RCC->CFGR |= (uint32_t)RCC_CFGR_HPRE_DIV1;
	/* PCLK = HCLK */
	RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE_DIV1;
	/* PLL configuration = HSI * 12 = 48 MHz */
	RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_PLLSRC |
					    RCC_CFGR_PLLXTPRE |
					    RCC_CFGR_PLLMULL));
	RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC_HSI_Div2 |
		RCC_CFGR_PLLXTPRE_PREDIV1 | RCC_CFGR_PLLMULL12);
	/* Enable PLL */
	RCC->CR |= RCC_CR_PLLON;
	/* Wait till PLL is ready */
	while((RCC->CR & RCC_CR_PLLRDY) == 0) { }

	/* Select PLL as system clock source */
	RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
	RCC->CFGR |= (uint32_t)RCC_CFGR_SW_PLL;    

	/* Wait till PLL is used as system clock source */
	while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) !=
	       (uint32_t)RCC_CFGR_SWS_PLL) { }
}


void	hw_init(void)
{
        RCC_ClocksTypeDef RCC_Clocks;
	RCC_GetClocksFreq(&RCC_Clocks);
	// Or use SystemCoreClock/48000
	setup_hse_clock();

	/* Urgh!  Remember to switch on everything we touch... */
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
	RCC->AHBENR |= RCC_AHBENR_GPIOBEN;

	flashvars_init();
        io_init();
	uart_init();
	time_init();
	spi_init();
	lightsense_init();
	input_init();

	printf("Hello das world!\n");

	__enable_irq();
}
