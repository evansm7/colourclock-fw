/* Copyright (c) 2014 Matt Evans
 *
 * lightsense:  Manage sampling the LDR and deciding whether to
 * brighten/dim the LEDs.
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

#include "lightsense.h"
#include <stm32f0xx.h>

#include "hw.h"
#include "time.h"
#include "uart.h"	// for printf, etc.
#include "flashvars.h"

// Bright light shining on it:
#define LDR_ADC_PRACTICAL_MAX	0x500
// The dark:
#define LDR_ADC_PRACTICAL_MIN	0x10

static uint8_t 	ldr_max;
static uint8_t 	ldr_min;

static time_callback_t tcb;
static uint16_t adc_val;

#define AVG_POS	16
static 	uint16_t	adc_avg[AVG_POS] = {0};
static 	uint16_t	adc_avg_val = 0;
static 	int		adc_sum = 0;
static 	int		adc_wr_pos = 0;

static void 	ls_tcb(uint64_t t_now)
{
	ADC1->CR |= ADC_CR_ADSTART;
}

void 	ADC1_COMP_IRQHandler(void)
{	
	uint32_t isr = ADC1->ISR;
	if (isr & ADC_ISR_EOC) {
		adc_val = ADC1->DR;

		adc_sum -= adc_avg[adc_wr_pos];
		adc_avg[adc_wr_pos] = adc_val;
		adc_sum += adc_val;
		adc_wr_pos = (adc_wr_pos + 1) & (AVG_POS - 1);
		adc_avg_val = adc_sum / AVG_POS;
	}
	if (isr & ADC_ISR_EOSEQ) {
		// Sequence of one...
		ADC1->ISR = ADC_ISR_EOSEQ;
	}
	// No more conversions in sequence
}

void debug_ls(void)
{
	static uint64_t tn = 0;
	static int tcnt = 0;
	static int samp = 0;

	if (time_getglobal() < (tn + 200)) {
		return;
	}

	tn = time_getglobal();
	printf("adc %04x   avg %04x  bri %03d\r\n", adc_val, adc_avg_val,
	       lightsense_get_brightness());
}

uint8_t lightsense_get_max(void)
{
	return ldr_max;
}

void 	lightsense_set_max(uint8_t d)
{
	ldr_max = d;
}

uint8_t lightsense_get_min(void)
{
	return ldr_min;
}

void 	lightsense_set_min(uint8_t d)
{
	ldr_min = d;
}

void	lightsense_save_limits(void)
{
	env.ls_max = ldr_max;
	env.ls_min = ldr_min;
	flashvars_update();
}

uint8_t	lightsense_get_brightness(void)
{
	// Scale between bright and 'dark' values to try to match
	// 0-255 as best we can.
	//
	// adc_avg_val is softened.
	int w = adc_avg_val;

	// Thresholds are just 8 bits, so *16 to get into the right range.
	w -= (ldr_min << 4);
	w = w * 256 / ((ldr_max - ldr_min) << 4);

	if (w > 255)
		return 255;
	if (w < 0)
		return 0;
	return w;
}

void 	lightsense_init(void)
{
	RCC->AHBENR |= LDR_GPIO_EN;
	GPIO_LDR->MODER |= 3 << (2*GPIO_LDR_CH);	// Analog mode
	GPIO_LDR->PUPDR &= ~(3 << (2*GPIO_LDR_CH));	// No pullup!

	/* HSI14 to clock ADC */
	RCC->CR2 |= RCC_CR2_HSI14ON;
	while (!(RCC->CR2 & RCC_CR2_HSI14RDY)) {}

	/* ADC clock on */
	RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

	ADC1->CR = 0;
	ADC1->CR |= ADC_CR_ADCAL;
	while (ADC1->CR & ADC_CR_ADCAL) {}
	adc_val = ADC1->DR;

	ADC1->CR |= ADC_CR_ADEN;
	while (!(ADC1->ISR & ADC_ISR_ADRDY)) {}

	ADC1->IER |= ADC_IER_EOSEQIE | ADC_IER_EOCIE;
	NVIC_EnableIRQ(ADC1_COMP_IRQn);
	NVIC_SetPriority(ADC1_COMP_IRQn, 0);

	/* Discontig; stop at end of sample sweep, wait for data read, auto
	 * off */
	ADC1->CFGR1 = ADC_CFGR1_DISCEN |
		ADC_CFGR1_AUTOFF |
		ADC_CFGR1_WAIT;
	/* RES bits = 00, so 12-bit. */

	ADC1->CFGR2 = 0;

	/* First channel to sample: */
	ADC1->CHSELR = B(ADC_LDR);

	/* Sampling time. */
	ADC1->SMPR = 
		ADC_SMPR1_SMPR_0 |
		ADC_SMPR1_SMPR_1 |
//		ADC_SMPR1_SMPR_2 |	// Arbitrary!
		0;

	// Read max/min thresholds from flash:
	if (flashvars_at_defaults()) {
		// These defaults are suitable for a build with
		// LDR and a 3mm opal covering:
		ldr_max = LDR_ADC_PRACTICAL_MAX / 16;
		ldr_min = LDR_ADC_PRACTICAL_MIN / 16;
	} else {
		ldr_max = env.ls_max;
		ldr_min = env.ls_min;
	}

	// Finally, a simple callback to periodically kick off a simple
	// sample:
	tcb.callback = ls_tcb;
	tcb.period = 50;           /* ms */
        time_callback_periodic(&tcb);
}
