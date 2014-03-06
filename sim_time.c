/* Copyright (c) 2014 Matt Evans
 *
 * sim_time:  Time routines for sim build.
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

#include <sys/time.h>
#include <inttypes.h>

uint64_t time_getglobal(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec*1000UL) + (tv.tv_usec/1000UL);
}
