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

#ifndef INPUT_H
#define INPUT_H

/* Input/UI system:
 * Follow basic 'click' events of buttons, but also
 * track 'holds' and long presses.
 */
void	input_init(void);

// Simple debounced buttons:
uint8_t	input_get_raw(void);


typedef enum { IE_NONE,
	       IE_LDOWN,
	       IE_LUP,
	       IE_LHOLD,
	       IE_LHOLDUP,
	       IE_RDOWN,
	       IE_RUP,
	       IE_RHOLD,
	       IE_RHOLDUP,
	       IE_BDOWN,
	       IE_BUP,
	       IE_BHOLD,
	       IE_BHOLDUP,
} InputEvent;

// Event consumption:
InputEvent	input_get_event(void);

#endif
