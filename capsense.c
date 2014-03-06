/* Copyright (c) 2014 Matt Evans
 *
 * capsense:  Experimental capacitance-measuring touch sensor.
 * This code isn't actually used by anything, but is included for
 * posterity and possibly educational value...
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

#include "hw.h"
#include "time.h"

#include "uart.h"	// for printf, etc.

// Capacitive touch sensor via analog inputs on PA0-3:
//
// Manual method.  Set pin to output, pull up to precharge.  Set neighbour (0/1
// and 2/3 paired) to output, pull-down to discharge slowly.  Set pin to ADC
// input; discharge begins.  Sample ADC input at a fixed interval later and
// determine how far the voltage has fallen.
//
// Acquire a 'touchedness' per channel.  Wrap this into a centre-point for
// touch, and over time determine delta of position.
//
// API:
// callbacks for
// - Single tap
// - Hold
// - scroll, ticks CW/CCW
//
// With new circular board:
// 12-bit result, seems to be large bias between channels (e.g 0 & 2 are about
// 0xe00 and 1 and 3 are about 0xf0/0x80).  Direct touch seems to make only
// about 8 values difference (over scale of 4096!)  Pulldown resistors are far
// far too strong?  68K.  Increase to 1M to test?



// Four channels sampled:
unsigned int	css[4] = {0};

uint16_t 	adc_cal = 0;

static int	cur_channel = 0;
static uint32_t	adc_val[4] = {0};

enum CSState 	{ S_PCHG, S_RELEASE, S_SAMPLING, S_DONE };
static int 	cs_state;

static time_callback_t tcb;

// Set PA[3:0] to output, value 0/1
static void	chan_out(int chan, int upndown)
{
	int mask = 3 << (chan & 3);

	GPIOA->MODER = (GPIOA->MODER & ~mask) | (mask & 0x55);
	GPIOA->BSRR = (1 << (chan & 3)) << (upndown ? 0 : 16);
}

static void	chan_adc_in(int chan)
{
	GPIOA->MODER |= 3 << (chan & 3);	// Analog mode
}

static void 	capsense_sampled_all(void)
{
	// All channels have been sampled (see adc_val[]).

	// Todo here....
	// Chart the samples-per-channel over time.
	//
	// If the channel with highest 'touch' moves, it's a tick in one direction.
	// Otherwise, if highest 'touch' remains for 1s, it's a 'hold'.
	// Otherwise, if it remains for <1s, it's a 'tap'.
	//
	// Fancy future todo:
	//
	// Acceleration of ticks (has been moving in the same
	// direction at roughly the same speed for the past $time)
}

static int neighbour_channel(int c)
{
	const int a[] = { 1, 0, 3, 2 };
	return a[c & 3];
}

// 5ms tick
static void 	capsense_tcb(uint64_t t_now)
{
	switch (cs_state) {
	case S_PCHG:
		// Channel gets pulled up to charge:
		chan_out(cur_channel, 1);
		// TO1 = PA0 = B
		// TO2 = PA1 = A
		// TO3 = PA2 = C
		// TO4 = PA3 = D
		// Neighbouring channel pulls down.
		//
		// A = 1 = 01 \
		// B = 0 = 00 /
		// C = 2 = 10 \
		// D = 3 = 11 /
		chan_out(neighbour_channel(cur_channel), 0);
		cs_state = S_RELEASE;
		break;
	case S_RELEASE:
		// The plan:  prepare ADC, then start timer.  Then set
		// ADC input and discharge phase begins:

		ADC1->CHSELR = B(cur_channel);
		ADC1->CR |= ADC_CR_ADSTART; 	// Triggered from TIM15
		cs_state = S_SAMPLING;

		TIM15->CR1 |= TIM_CR1_CEN;	// Timer starts

		// Set to input; now starts discharging
		chan_adc_in(cur_channel);

		// ADC IRQ will set adc_val[cur_channel]
		break;
	case S_SAMPLING:
		break;
	case S_DONE:
//		if (++cur_channel > 3) {
//			capsense_sampled_all();
//			cur_channel = 0;
//		}
		cs_state = S_PCHG;
		break;
	}
}

void 	ADC1_COMP_IRQHandler(void)
{	
	uint32_t isr = ADC1->ISR;
	if (isr & ADC_ISR_EOC) {
		adc_val[cur_channel] = ADC1->DR;
	}
	if (isr & ADC_ISR_EOSEQ) {
		// Sequence of one...
		ADC1->ISR = ADC_ISR_EOSEQ;
		cs_state = S_DONE;
	}
	// No more conversions in sequence
}

void	debug_cs(void)
{
	static uint64_t tn = 0;
	static int tcnt = 0;
	static int samp = 0;

	if (time_getglobal() < (tn + 200)) {
		return;
	}

	tn = time_getglobal();
	printf(
//		"CSS %04x:%04x:%04x:%04x "
	       " adc %04x:%04x:%04x:%04x  ch %d samp %d\r\n",
//	       css[0], css[1], css[2], css[3],
	       adc_val[0], adc_val[1], adc_val[2], adc_val[3],
	       cur_channel,
	       samp
	       );
	
	tcnt++;
	if (tcnt == 5*2) {
		tcnt = 0;
		if (++samp == 8)
			samp = 0;
		
		ADC1->SMPR = samp & ADC_SMPR1_SMPR;
	}
}

void	capsense_init(void)
{
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN;

	/* PA[3:0] */
	GPIOA->PUPDR &= ~0xff;		/* disable pullup/downs */
	GPIOA->OTYPER &= ~0xf; 		/* Outputs are push-pull */
	GPIOA->OSPEEDR |= 0xff; 	/* 50MHz/high current */

	/* HSI14 to clock ADC */
	RCC->CR2 |= RCC_CR2_HSI14ON;
	while (!(RCC->CR2 & RCC_CR2_HSI14RDY)) {}

	/* ADC clock on */
	RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

	ADC1->CR = 0;
	ADC1->CR |= ADC_CR_ADCAL;
	while (ADC1->CR & ADC_CR_ADCAL) {}
	adc_cal = ADC1->DR;

	ADC1->CR |= ADC_CR_ADEN;
	while (!(ADC1->ISR & ADC_ISR_ADRDY)) {}

	ADC1->IER |= ADC_IER_EOSEQIE | ADC_IER_EOCIE;
	NVIC_EnableIRQ(ADC1_COMP_IRQn);
	NVIC_SetPriority(ADC1_COMP_IRQn, 0);

	/* Discontig; stop at end of sample sweep, wait for data read, auto
	 * off */
	ADC1->CFGR1 = ADC_CFGR1_DISCEN |
		ADC_CFGR1_AUTOFF |
		ADC_CFGR1_WAIT |
		ADC_CFGR1_EXTEN_0 |
		ADC_CFGR1_EXTSEL_2;
	/* RES bits = 00, so 12-bit.  Exten=01, rising edge trigger.
	 * ExtSel=100 = TRG4 = TIM15_TRGO.
	 */

	ADC1->CFGR2 = 0;

	/* First channel to sample: */
	ADC1->CHSELR = B(0);

	/* Sampling time.
	 * Empirically, 001 7.5 cycles (14MHz, ~0.5us) gives highest
	 * difference between touch/no-touch.
	 */
	ADC1->SMPR = 
		ADC_SMPR1_SMPR_0 |
//		ADC_SMPR1_SMPR_1 |
//		ADC_SMPR1_SMPR_2 |
		0;

	cur_channel = 0;
	cs_state = S_PCHG;

	// Set up Timer TIM15 to provide trigger to ADC conversion.  This allows
	// a much more carefully-controlled time between setting ADC input
	// (i.e. remove pullup, start discharge of external capacitance) and
	// sample:

	const unsigned int timer_delay = 24*100; 	// 24MHz ticks

	/* TIM15 clock on; one-shot (stop on rollover) and TRGO on rollover: */
	RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;
	TIM15->EGR = 0;
	TIM15->CCMR1 = 0;
	TIM15->CCER = 0;
	TIM15->DIER = 0;		// No IRQs.
	TIM15->SR = 0;

	TIM15->PSC = 1; 		// Periph48/2, so 24MHz timer clock
	TIM15->ARR = timer_delay; 	// Roll over here.
	TIM15->CNT = 0;

	TIM15->CR1 = TIM_CR1_OPM | 	// One-shot
		TIM_CR1_URS | // Only ovf makes irq
		TIM_CR1_ARPE;
	TIM15->CR2 = TIM_CR2_MMS_1; // MMS=010 = Update/rollover causes TRGO

	// So, after this setup, we select a channel, set ADSTART=1 then
	// when TIM15 fires, the ADC conversion begins.
	// Start by setting TIM15->CR1.TIM_CR1_CEN.

	tcb.callback = capsense_tcb;
	tcb.period = 5;           /* ms */
        time_callback_periodic(&tcb);
}
