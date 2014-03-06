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

#ifndef SPI_H
#define SPI_H

#include <inttypes.h>

void spi_init(void);
void spi_tx64(uint16_t d0, uint16_t d1, uint16_t d2, uint16_t d3);
void spi_tx_sync();

void dma_tx(uint16_t *data_ptr);
void dma_sync();

#endif
