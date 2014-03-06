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

#ifndef LED_DISP_H
#define LED_DISP_H

#include "types.h"

void	led_disp_init(void);
void	led_test(void);
// The offset flag causes pixel '0' to appear at 12o'clock (third quadrant, centre pixel)
// such that the MCU hangs at the bottom, with the drill hole at 6o'clock:
void	led_fb_to_pwm_buffer(pix_t *fb, int offset_to_12oclock);

// Wait for vsync/swap double buffers.  (Internally, does WFI.)
void	led_fb_vsync_swap(void);

#endif
