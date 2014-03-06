/* Copyright (c) 2014 Matt Evans
 *
 * led_disp: scan the Colourclock LED drivers, perform PWM and manage
 * framebuffer updates.
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
#include <stm32f0xx_tim.h>

#include "types.h"
#include "hw.h"
#include "led_disp.h"
#include "time.h"
#include "spi.h"
#include "lightsense.h"

// Internal IRQ handler state:
static int scan_third = 0;
static int cur_arr_idx = 0;
static int cur_step = 0;

static volatile int dma_irqs = 0;
static volatile int timer_irqs = 0;
static volatile unsigned int userspins = 0;

// At the end, N 'ticks' are all black, to give time for pullups to
// settle/discharge/etc. (Observed significant 'bleed' from one run through to
// the next on a different common pullup, first FET was not switching off fully
// before second FET was switched on.)
#define PWM_DEAD_TIME 	1

#define PWM_SHIFT 	6
#define PWM_STEPS 	(1<<PWM_SHIFT)
#define PWM_REFRESH_HZ 	300
#define PWM_TIMER_HZ 	(PWM_REFRESH_HZ * 3 * (PWM_STEPS + PWM_DEAD_TIME))

// This buffer holds 'flattened' bitstream data that's sent to the driver shift
// regs via SPI ever 'PWM tick'.  When the framebuffer is updated, this buffer
// is recalculated, rather than doing the maths every interrupt.
static uint16_t pwm_data[2][3][4 * (PWM_STEPS + PWM_DEAD_TIME)];
// Common sizes:  6-bit PWM/64 levels ~= 1.5KB
// Common sizes:  7-bit PWM/128 levels ~= 3KB

// (Advantageous to double-buffer this pwm_data array!)  May just get away with
// 6K out of 8

static int 		buf_wr = 0;
static volatile int 	buf_rd = 1;

static inline int led_buffer_new(void)
{
	return buf_wr == buf_rd;
}

static void led_buffer_swap(void)
{
	// If a new buffer is pending, swap ptr & use new one
	if (led_buffer_new()) {
		buf_rd = 1 ^ buf_rd;
	}
}

void led_fb_vsync_swap(void)
{
	// 2 ptrs; displaying & writing.
	// write fb_data into off-screen buffer[writing]
	// then update writing = displaying.
	// Then, we wait for them to become non-equal, in wfi().
	// IRQ sees they're equal and 'flips', so displaying = !displaying
	// Then, we're done.

	buf_wr = 1 ^ buf_wr;
	while (led_buffer_new()) { __WFI(); }
}

static inline void a_io(int bit, int on)
{
	GPIOA->BSRR = B(bit) << ( on ? 0 : 16 );
}

static inline void b_io(int bit, int on)
{
	GPIOB->BSRR = B(bit) << ( on ? 0 : 16 );
}


// LED position (0-4 in group of 5) to driver bit map, for R/G/B colours:
static const unsigned char led_rgb_to_bit[3][5] = {
	/* R */
	{ 12, 10, 6, 3, 0 },
	/* G */
	{ 13, 9,  7, 4, 1 },
	/* B */
	{ 14, 8, 11, 5, 2 }
};

// Framebuffers generally run from 12o'clock CW; when hung, the clock's quadrant
// 2 (third) is at the top and the 12o'clock pixel (pixel 0) is LED 36.  Convert
// an index from one to the other:
static int rotate_offset(int o)
{
	o -= 7 + 30;
	if (o < 0)
		o += 60;
	return o;
}

void	led_fb_to_pwm_buffer(pix_t *fb, int offset_to_12oclock)
{
	/* Transform RGB values in 'framebuffer' into data that can be directly
	 * (and quickly) clocked out to the drivers.
	 */
	for (int third = 0; third < 3; third++) {
		int po = 0;
		for (int ps = 0; ps < PWM_STEPS; ps++) {
			uint16_t *w = &pwm_data[buf_wr][third][po];

			for (int quadrant = 0; quadrant < 4; quadrant++) {
				w[quadrant] = 0;
				for (int i = 0; i < 5; i++) {
					// quadrant 0 is actually the most CW
					// one, quadrant 3 being the 'start'
					// (MCU)
					int pix_in_quad = ((3-quadrant)*15)+i;
					int pix_idx = pix_in_quad + (third * 5);

					if (offset_to_12oclock)
						pix_idx = rotate_offset(pix_idx);

					// The words are shifted out MSB first, so
					// Driver bit 15 means bit shifted out first
					if ((fb[pix_idx].r>>(8-PWM_SHIFT)) > ps) {
						w[quadrant] |= 1 << led_rgb_to_bit[0][i];
					}
					if ((fb[pix_idx].g>>(8-PWM_SHIFT)) > ps) {
						w[quadrant] |= 1 << led_rgb_to_bit[1][i];
					}
					if ((fb[pix_idx].b>>(8-PWM_SHIFT)) > ps) {
						w[quadrant] |= 1 << led_rgb_to_bit[2][i];
					}
				}
			}
			po += 4;
		}
		for (int i = 0; i < PWM_DEAD_TIME; i++) {
			// The final burst output is a dark gap so that the
			// common/FET pullups can be altered without messing with /OE:
			pwm_data[buf_wr][third][po++] = 0;
			pwm_data[buf_wr][third][po++] = 0;
			pwm_data[buf_wr][third][po++] = 0;
			pwm_data[buf_wr][third][po++] = 0;
		}
	}
}

// /OE is on PB15, which is TIM1_CH3N, TIM15_CH1N, TIM15_CH2.
static void	led_pwm_brightness_init(void)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_OCInitTypeDef	TIM_OCInitStructure;

	RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
	GPIOB->MODER &=  ~0xc0000000; //
	GPIOB->MODER |=   0x80000000; // PB15 AF

	GPIOB->OSPEEDR |= 0xc0000000; // fast driver
	GPIOB->AFR[1] &= ~0xf0000000; //
	GPIOB->AFR[1] |=  0x20000000; // AF2 = TIM1_CH3N

	// TIM1_CH3N output
	// 0-100% duty; 128 ticks at 24MHz = 375KHz min
	RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;

	TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
	TIM_TimeBaseStructure.TIM_Prescaler = 0; 	// 24MHz?
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

	TIM_TimeBaseStructure.TIM_Period = 256;		// ARR
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;

	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);
	TIM_ARRPreloadConfig(TIM1, ENABLE);

	/* Channel 1 output configuration.  Pay attention to 'N' in enums, as,
	 * for example, TIM_OutputState_Enable hasn't got the same value as
	 * TIM_OutputNState_Enable :-( (1h wasted)
	 */
	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
	TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable;
	// This is 'time low until high'
	// This is CCR3:
	TIM_OCInitStructure.TIM_Pulse = 0;	// Initially off.
	TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
	TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Reset;

	TIM_OC3Init(TIM1, &TIM_OCInitStructure);
	TIM_OC3PreloadConfig(TIM1, TIM_OCPreload_Enable);

	/* TIM1 Main Output Enable */
	TIM_CtrlPWMOutputs(TIM1, ENABLE);

	/* TIM1 counter enable */
	TIM_Cmd(TIM1, ENABLE);

	// CCER = CC1NE=1, CC1NP=0
}

// Adjust overall LED brightness, on a scale of 0-255.
void	led_brightness(uint8_t b)
{
	TIM1->CCR3 = b;///2;
}

static time_callback_t tcb;

static void 	led_bri_tcb(uint64_t t_now)
{
	int i = lightsense_get_brightness();

	// Don't switch off completely in darkness.
	if (i == 0)
		i = 1;
	// TODO:  Could do some kind of non-linear transformation/lookup
	//	  here to scale for perception or taste.

	led_brightness(i);
}

void	led_disp_init(void)
{
	// Init outputs
	a_io(A_SI, 0);
	a_io(A_CK, 0);
	b_io(B_SA, 0);
	b_io(B_SB, 0);
	b_io(B_SC, 0);
	b_io(B_LE, 0);
	b_io(B_OE, 0);	// On

        /* Power on DMA */
        RCC->AHBENR |= RCC_AHBENR_DMA1EN;

        /* Power on TIM14 */
	RCC->APB1ENR |= RCC_APB1ENR_TIM14EN;

	/* Zero PWM data buffer */
	for (int b = 0; b < 2; b++) {
		for (int t = 0; t < 3; t++) {
			for (int i = 0; i < PWM_STEPS+PWM_DEAD_TIME; i++) {
				pwm_data[b][t][i] = 0;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////
	// Set up DMA:

	// High-prio 16 bit transfers (both sides), MINC but no PINC, DIR=1
	// (write periph)
	DMA1_Channel3->CCR = DMA_CCR_PL_1 |
		DMA_CCR_MSIZE_0 | DMA_CCR_PSIZE_0 |
		DMA_CCR_MINC | DMA_CCR_DIR |
		DMA_CCR_TCIE;
	DMA1->IFCR = DMA_ISR_TCIF3;
	DMA1_Channel3->CPAR = (uintptr_t)&SPI1->DR;

	scan_third = 0;
	cur_arr_idx = 0;
	cur_step = 0;

	NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
        NVIC_SetPriority(DMA1_Channel2_3_IRQn, 0);

	//////////////////////////////////////////////////////////////////////
	// Set up Timer14:
	TIM14->EGR = 0;
	TIM14->CCMR1 = 0;
	TIM14->CCER = 0;
	TIM14->DIER = TIM_DIER_UIE;
	TIM14->SR = 0;

	// Periph clock is 48MHz
	TIM14->PSC = 1; 	// /2, so 24MHz timer clock
	TIM14->ARR = 24000000/PWM_TIMER_HZ;
	TIM14->CNT = 0;

	TIM14->CR1 = TIM_CR1_URS | // Only ovf makes irq
		TIM_CR1_ARPE |
		TIM_CR1_CEN;

	NVIC_EnableIRQ(TIM14_IRQn);
        NVIC_SetPriority(TIM14_IRQn, 0);

	// Timer now running, triggering DMA to SPI for sink driver shift
	// registers.

	led_pwm_brightness_init();

	// Simple timer callback to scale brightness by LDR input:
	tcb.callback = led_bri_tcb;
	tcb.period = 50; /* ms */
        time_callback_periodic(&tcb);
}

////////////////////////////////////////////////////////////////////////

void DMA1_Channel2_3_IRQHandler(void)
{
	if (DMA1->ISR & DMA_ISR_TCIF3) {
		// Ensure that the SPI has finished dropping out the last data:
		while (SPI1->SR & SPI_SR_BSY) {}
		// Strobe in that data ASAP:
		b_io(B_LE, 1);
		b_io(B_LE, 0);

		// Clear the pending IRQ
		DMA1->IFCR = DMA_ISR_TCIF3;

		// Next transfer is kicked off by timer.

		// If this was the first 'dead time' step, though, switch off the FETs:
		if (cur_step > PWM_STEPS) {
			GPIOB->BSRR = 7 << (B_SA + 16);	// ABC off
		}
	}
	dma_irqs++;
}

void TIM14_IRQHandler(void)
{
	if (cur_step == (PWM_STEPS+PWM_DEAD_TIME)) {
		cur_step = 0;
		// Wrapped, move to next third:
		if (++scan_third == 3) {
			led_buffer_swap();
			scan_third = 0;
		}

		cur_arr_idx = 0;

		// The first 'dead time' cycle outputs 0 to the drivers and,
		// once latched in the DMA completion IRQ above, the FETs are
		// switched off.

		// The last 'dead time' cycle (here) switches on the FET for the
		// next third:
		GPIOB->BSRR = (1 << (B_SA + scan_third));	// On
	} else {
		/* Arrange for 4 halfwords to be DMAd
		 * to SPI:
		 */
		DMA1_Channel3->CCR &= ~DMA_CCR_EN;
		// This data will be all zeros when cur_step >= PWM_STEPS (i.e.
		// dead-time cycle)
		DMA1_Channel3->CMAR = (uintptr_t)&pwm_data[buf_rd][scan_third][cur_arr_idx];
		DMA1_Channel3->CNDTR = 4;	// Num de halfwords
		cur_arr_idx += 4;
		cur_step++;

		// Seems to need to be set separately:
		DMA1_Channel3->CCR |= DMA_CCR_EN;

		timer_irqs++;
		TIM14->SR = 0;
	}
}

void led_test(void)
{
	pix_t fb[60];

	// Set up a test pattern:
	for (int b = 0; b < 60; b++) {
//#define SIMPLE
#ifdef SIMPLE	// Basic test
		fb[b].r = 0;//b*2;
		fb[b].g = 0;//(b*4);// & (b & 0x20);
		fb[b].b = 0;//b*4;

		// This test pattern is useful for seeing inter-third 'bleed'
		// where the common pullups allow ghosting between scans:
		fb[0].r = 256/PWM_STEPS;	// lowest 'notch'
		fb[6].g = 256/PWM_STEPS;
		fb[12].b = 256/PWM_STEPS;

		fb[15+0] = 256/PWM_STEPS;	// lowest 'notch'
		fb[15+6] = 256/PWM_STEPS;
		fb[15+12] = 256/PWM_STEPS;

		fb[30+0] = 256/PWM_STEPS;	// lowest 'notch'
		fb[30+6] = 256/PWM_STEPS;
		fb[30+12] = 256/PWM_STEPS;

		fb[45+0] = 256/PWM_STEPS;	// lowest 'notch'
		fb[45+6] = 256/PWM_STEPS;
		fb[45+12] = 256/PWM_STEPS;
#else
		// Nice blend:
		fb[b].r = b*4;
		fb[b].g = (b > 30) ? b*2 : 0;
		fb[b].b = 236-(b*4);
#endif
	}

	led_fb_to_pwm_buffer(fb, 0);		// No offset, raw pixel positions

	// Opportunity to test performance/overhead of IRQ PWM.

	// The 'userspins++' loop below, with no PWM IRQs active, gives baseline of
	// 5.326M userspin loops/sec.

	// PWM at N levels, SPI clock at 24MHz (/2), 1 cycle deadtime:
	// 64 @200Hz: 	4.32M spins/s 	(IRQs 19% CPU time)
	// 64 @300Hz: 	3.81M spins/s	(IRQs 29% CPU time)
	// 128 @200Hz:	3.31M spins/s 	(IRQs 48% CPU time)

	// However, 128 levels doesn't look vastly different (on LEDs, since rather
	// non-linear and 'bright') to 64, so choose 64@300Hz.

	while(1) {
		userspins++;
	}
}

static void testloop(void)
{
	int c = 0;

	b_io(B_OE, 0); // LEDs on

	while(1) {
		// PWM: Three 'thirds' are present, selected by commons A,B,C.
		// Each quadrant is divided into 3.  (So, 15/3 = 5 (RGB) LEDs
		// per third.)  At any one time, a common energises a third in
		// each quadrant, i.e. 5*4 LEDs.
		//
		// Output 20 LEDs worth, PWM these, then switch to next third
		// and repeat.

		for (c = 0; c < 3; c++) {
			uint64_t w;
			int pwm_arr_idx = 0;

			b_io(B_SA, (c == 0));
			b_io(B_SB, (c == 1));
			b_io(B_SC, (c == 2));

			for (int ac = 0; ac <= PWM_STEPS; ac++) {
				// 225Hz @64 levels, 450 @32
#if 0
				spi_tx64(pwm_data[pwm_arr_idx++],
					 pwm_data[pwm_arr_idx++],
					 pwm_data[pwm_arr_idx++],
					 pwm_data[pwm_arr_idx++]);
				spi_tx_sync();
#else
				dma_tx(&pwm_data[0][c][pwm_arr_idx]);
				dma_sync();
				pwm_arr_idx += 4;
				spi_tx_sync();
#endif
				b_io(B_LE, 1);
				b_io(B_LE, 0);

				// Bit positions in each halfword accord with
				// led_rgb_to_bit

				// With MCU at 6'oclock, quadrants go clockwise
				// from MCU, with MCU quadrant being last
				// halfword to be sent.



				// IRQs:
				// start DMA, get transfer-done interrupt
				// then wait for SPI !BSY?  Or TXE interrupt then
				// !BSY?
			}
		}
	}
}
