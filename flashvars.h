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

#ifndef FLASHVARS_H
#define FLASHVARS_H

#include <inttypes.h>

typedef struct {
	uint16_t	_eyecatcher;
	uint8_t		ls_max;
	uint8_t		ls_min;
} FlashVars;

extern FlashVars env;

void	flashvars_init(void);
int	flashvars_at_defaults(void);
void	flashvars_update(void);
	
#endif
