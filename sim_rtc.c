/* Copyright (c) 2014 Matt Evans
 *
 * sim_rtc:  Clock routines for sim build
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

#include "rtc.h"
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>

static int go_fast = 0;

enum clk_type {
	REAL_TIME,
	START_MIDNIGHT,
	RANDOM,
};

static int run_type = RANDOM;

////////////////////////////////////////

static uint64_t tus_start;

void rtc_init(void)
{
	struct timeval tv_start;
	gettimeofday(&tv_start, NULL);
	srandom(tv_start.tv_sec);

	char *tv = getenv("RUN_TYPE");
	if (tv) {
		if (strcmp(tv, "REAL") == 0)
			run_type = REAL_TIME;
		else if (strcmp(tv, "MIDNIGHT") == 0)
			run_type = START_MIDNIGHT;
		else if (strcmp(tv, "RANDOM") == 0)
			run_type = RANDOM;
	}
	if (run_type == RANDOM) {
		tv_start.tv_sec -= (random() % (60*60*24));
	}
	tus_start = (tv_start.tv_sec*1000000) + tv_start.tv_usec;

	char *fv = getenv("RUN_FAST");
	if (fv) {
		go_fast = atoi(fv);
	}
}

void rtc_gettime(tod_t *time_out)
{
	int i;
	struct timeval now;
	uint64_t tus_now; 

	gettimeofday(&now, NULL);

	tus_now = (now.tv_sec*1000000) + now.tv_usec;

	if (run_type != REAL_TIME) {
		tus_now -= tus_start;

		if (go_fast) {
			tus_now *= go_fast;
		}
	}

	// Absolute time to TOD:
	time_out->hour 		= ((tus_now/1000000) % (12*60*60)) / (60*60);
	time_out->min   	= ((tus_now/1000000) % (60*60)) / 60;
	time_out->sec 		= (tus_now/1000000) % 60;
	time_out->subsec 	= ((tus_now*64) % 64000000)/1000000;
	time_out->amnpm 	= (((tus_now/1000000) % (24*60*60)) / (60*60)) >= 12;
}

void rtc_settime(tod_t *time_out)
{
}
