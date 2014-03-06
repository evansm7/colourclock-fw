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

#ifndef lookuptables_H
#define lookuptables_H

#define SINTAB_SHIFT 15		/* Note: range -1/+1 in a 16-bit type */
#define SINTAB_ENTRIES 512 /* 512 steps for 360 degrees; could obv. only store 90deg... */
extern const short sintab[];

/* These can easily flip bits in index to v. quickly look up in table of 0-90degrees. */
#define SIN(x) ((int)sintab[(x) & (SINTAB_ENTRIES-1)])
#define COS(x) ((int)sintab[((x) + (SINTAB_ENTRIES/4)) & (SINTAB_ENTRIES-1)])


#endif
