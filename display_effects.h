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

#ifndef DISPLAY_EFFECTS_H
#define DISPLAY_EFFECTS_H

#include "rtc.h"

void 	display_init(void);
void 	display_draw(pix_t *fb, unsigned int framenum, tod_t *time);
void 	display_next(void);
void 	display_prev(void);

typedef enum { DS_HR, DS_MIN, DS_BR_H, DS_BR_L } DispType;

void 	display_drawspecial(pix_t *fb,
			    DispType t,
			    unsigned int param_a,
			    unsigned int param_b);

#endif
