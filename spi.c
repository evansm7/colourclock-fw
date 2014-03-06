/* Copyright (c) 2014 Matt Evans
 *
 * spi:  SPI setup and data transfer routines (including DMA)
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

/* PA7 = MOSI
 * PA5 = SCK
 *
 * SPI peripheral used to output 64 bits (to 4x 16-bit drivers), manual LE strobe
 */

void spi_init(void)
{
        /* SSM = 0 for software NSS control
         * Set AF=0 for PA5 (clk) & PA7 (MOSI)
         * CPOL=0
         * CPHA=0 (normally-zero CLK, rising edge latches)
         * BR[2:0] -> /4 for 12MHz?  /2 would also work fine, drivers are fast
         * 				enough.
         * BIDIMODE
         * BIDIOE yes (output unless specific input)
         * DS = 9 bits
         * LSBFIRST
         * SSM=1, SSI=1, SSOE=x
         * MSTR=1
         */
        /* Power on SPI periph! */
        RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

        GPIOA->MODER = (GPIOA->MODER & ~(B2(0x3, 7) | B2(0x3, 5))) |
                B2(0x2, 7) | B2(0x2, 5); // AF for A5/A7
        GPIOA->AFR[0] &= ~0xf0f00000; // AF 0

        SPI1->CR1 = SPI_CR1_BIDIMODE
                | SPI_CR1_BIDIOE
                | SPI_CR1_SSM
                | SPI_CR1_SSI           // Set SSI & unset when reading?
//		| SPI_CR1_BR_0  // /4, 12MHz
//		| SPI_CR1_BR_1  // /8
                | SPI_CR1_MSTR;

        SPI1->CR2 = SPI_CR2_DS_3 | SPI_CR2_DS_2 |
		SPI_CR2_DS_1 | SPI_CR2_DS_0; // 16-bit data
        SPI1->CR2 |= SPI_CR2_TXDMAEN;
        SPI1->CR1 |= SPI_CR1_SPE;
}

static inline int spi_can_tx(void)
{
        // FIFO 32 bits, so can TX if 1/2 full or empty, i.e. not full:
        /* return (SPI1->SR & (SPI_SR_FTLVL_0 | SPI_SR_FTLVL_1)) != */
        /*         (SPI_SR_FTLVL_0 | SPI_SR_FTLVL_1); */
	return SPI1->SR & SPI_SR_TXE;
}

static inline void spi_tx16(uint16_t d)
{
	// Shifted out MSB first
        while (!spi_can_tx()) {}
        SPI1->DR = d;
}

void spi_tx64(uint16_t d0, uint16_t d1, uint16_t d2, uint16_t d3)
{
	// MSB first
	spi_tx16(d0);
	spi_tx16(d1);
	spi_tx16(d2);
	spi_tx16(d3);
}

void spi_tx_sync(void)
{
	while (SPI1->SR & SPI_SR_BSY) {}
}

unsigned int waitspins = 0;

void dma_tx(uint16_t *data_ptr)
{
	if (DMA1_Channel3->CCR & DMA_CCR_EN) {
		/* If DMA still enabled, wait for TCIF,
		 * DMA Done:
		 */
		while (!(DMA1->ISR & DMA_ISR_TCIF3)) {
			/* DO SOMETHING USEFUL HERE */
			waitspins++;
		}
		DMA1_Channel3->CCR = 0x2592;
	}

	/* Now arrange for 4 halfwords to be DMAd
	 * to SPI:
	 */
	DMA1->IFCR = DMA_ISR_TCIF3; /* Clear C3 TCIF */
	DMA1_Channel3->CPAR = (uintptr_t)&SPI1->DR;
	DMA1_Channel3->CMAR = (uintptr_t)data_ptr;
	DMA1_Channel3->CNDTR = 4;	// Num de halfwords?
	/* Hi prio 16 bit transfers (both sides),
	 * MINC but no PINC, DIR=1 (write periph)
	 */
	DMA1_Channel3->CCR = 0x2592;

	// interrupt when done?

	/* Go! */
	DMA1_Channel3->CCR = 0x2593;
}

void dma_sync(void)
{
	if (DMA1_Channel3->CCR & DMA_CCR_EN) {
		/* If DMA still enabled, wait for TCIF,
		 * DMA Done:
		 */
		while (!(DMA1->ISR & DMA_ISR_TCIF3)) {
			/* DO SOMETHING USEFUL HERE */
			waitspins++;
		}
		DMA1_Channel3->CCR = 0x2592;
	}
}
