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

#ifndef LIGHTSENSE_H
#define LIGHTSENSE_H

#include <inttypes.h>


void 	lightsense_init(void);
uint8_t	lightsense_get_brightness(void);

#ifdef SIM
static uint8_t lightsense_get_max(void) 	{ return 200; }
static uint8_t lightsense_get_min(void)	 	{ return 10; }
static void 	lightsense_set_max(uint8_t d)   {}
static void 	lightsense_set_min(uint8_t d)   {}
static void	lightsense_save_limits(void)    {}
#else
// Flexible max/min thresholds from LDR:
uint8_t lightsense_get_max(void);
uint8_t lightsense_get_min(void);
void 	lightsense_set_max(uint8_t d);
void 	lightsense_set_min(uint8_t d);
void	lightsense_save_limits(void);
#endif

#endif
