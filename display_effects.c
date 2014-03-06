/* Copyright (c) 2014 Matt Evans
 *
 * display_effects:  Turn time into pretty pixels in our circular "1D"
 * framebuffer.
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

#include "types.h"
#include "rtc.h"
#include "lookuptables.h"
#include "display_effects.h"

#ifdef SIM
#include <stdlib.h>
#else
#include <stm32f0xx_rtc.h>
#endif

static unsigned int current_disp = 0;

typedef void (*dispfunc)(pix_t *fb, unsigned int framenum, tod_t *time, int param);

////////////////////////////////////////////////////////////////////////////////

static uint8_t sat_add8(uint8_t a, uint8_t b)
{
	int i = a+b;
	if (i > 255)
		return 255;
	else
		return i;
}

////////////////////////////////////////////////////////////////////////////////

void d_ticks(pix_t *fb, int bright)
{
	int i;
	for (i = 0; i < 60; i += 15) {
		int j = (i == 0) ? bright : bright/4;
		fb[i].r = sat_add8(fb[i].r, j);
		fb[i].g = sat_add8(fb[i].g, j);
		fb[i].b = sat_add8(fb[i].b, j);
	}
}

void d_pie(pix_t *fb, unsigned int framenum, tod_t *time, int param)
{
	int 		i;
	const int	piewidth = 12;
	int		h, m, s;
	int		st_s, st_m, st_h;
	int 		fr_s, fr_m, fr_h;

	// Convert the time into angular quantities from TDC:
	s = time->sec*64 + time->subsec;	// 60*64
	m = time->min*64 + (s/60);	       	// 60*64
	h = (5*time->hour*64) + (5*m/60);      	// 60*64

	// Start points and fractions for each of the hands.
	st_s = (s/64);
	st_m = (m/64);
	st_h = (h/64);

	if (param & 2) {
		fr_s = s % 64;
		fr_m = m % 64;
		fr_h = h % 64;
	} else {
		// A 'sharp' tick: just ignore the fractional mid-pixel
		// position:
		fr_s = 0;
		fr_m = 0;
		fr_h = 0;
	}

	// Draw on a black background:
	for (i = 0; i < 60; i++) {
		int j;

		fb[i].r = 0;
		fb[i].g = 0;
		fb[i].b = 0;
	}

	// Hours
	for (i = 0; i < piewidth; i++) {
		int br_h = 255 - (i * (256/piewidth) + ((fr_h*256/piewidth)/64));
		int p_h = (st_h - i) % 60;
		if (p_h < 0)
			p_h += 60;
		fb[p_h].r = br_h;
	}
	// An extra leading edge pixel, if we're not exactly on a tick:
	if (fr_h) {
		int p_h = (st_h + 1) % 60;
		fb[p_h].r = fr_h * 4;
	}

	// Mins
	for (i = 0; i < piewidth; i++) {
		int br_m = 255 - (i * (256/piewidth) + ((fr_m*256/piewidth)/64));
		int p_m = (st_m - i) % 60;
		if (p_m < 0)
			p_m += 60;
		fb[p_m].g = br_m;
	}
	// An extra leading edge pixel, if we're not exactly on a tick:
	if (fr_m) {
		int p_m = (st_m + 1) % 60;
		fb[p_m].g = fr_m * 4;
	}

	// Secs
	for (i = 0; i < piewidth; i++) {
		int br_s = 255 - (i * (256/piewidth) + ((fr_s*256/piewidth)/64));
		int p_s = (st_s - i) % 60;
		if (p_s < 0)
			p_s += 60;
		fb[p_s].b = br_s;
	}
	// An extra leading edge pixel, if we're not exactly on a tick:
	if (fr_s) {
		int p_s = (st_s + 1) % 60;
		fb[p_s].b = fr_s * 4;
	}

	if (param & 1) {
		d_ticks(fb, 32);
	}
}

void d_simple_soft(pix_t *fb, unsigned int framenum, tod_t *time, int param)
{
	int i;

	int bi = 8+((8*SIN(framenum))>>SINTAB_SHIFT);
	int bj = 8+((8*SIN(framenum*2))>>SINTAB_SHIFT);
	int bk = 8+((8*SIN(framenum*3))>>SINTAB_SHIFT);


#ifdef PSYCHEDELIC_BACKGROUND_BUT_WEIRD_ON_LEDS
	for (i = 0; i < 60; i++) {
		int c;
		c = bi+((16*SIN(framenum + (5*i*SINTAB_ENTRIES/60))>>SINTAB_SHIFT));
		fb[i].r = c > 0 ? c : 0;
		c = bj+((16*SIN(framenum + (2*i*SINTAB_ENTRIES/60))>>SINTAB_SHIFT));
		fb[i].g = c > 0 ? c : 0;
		c = bk+((16*SIN(framenum + (3*i*SINTAB_ENTRIES/60))>>SINTAB_SHIFT));
		fb[i].b = c > 0 ? c : 0;
	}
#else
	for (i = 0; i < 60; i++) {
		fb[i].r = 0;
		fb[i].g = 0;
		fb[i].b = 0;
	}
#endif

	if (param & 2) {
		// h/m/s as 6-bit fraction
		int h, m, s;

		s = time->sec*64 + time->subsec;	// 60*64
		m = time->min*64 + (s/60);	       	// 60*64
		h = time->hour*5*64 + (5*m/60);		// 12*64

		int j,k;

		j = s / 64;	// First LED this tick is present on
		k = s % 64;	// How far between
		fb[j].b = sat_add8(fb[j].b, 255*(64-k)/64);
		fb[(j+1) % 60].b = sat_add8(fb[(j+1) % 60].b, 255*k/64);

		j = m / 64;
		k = m % 64;
		fb[j].g = sat_add8(fb[j].g, 255*(64-k)/64);
		fb[(j+1) % 60].g = sat_add8(fb[(j+1) % 60].g, 255*k/64);

		j = h / 64;
		k = h % 64;
		fb[j].r = sat_add8(fb[j].r, 255*(64-k)/64);
		fb[(j+1) % 60].r = sat_add8(fb[(j+1) % 60].r, 255*k/64);
	} else {
		fb[time->sec].b = 255;
		fb[time->min].g = 255;
		fb[((time->hour * 60) + time->min)/12].r = 255;
	}	

	if (param & 1) {
		d_ticks(fb, 32);
	}
}

void d_blob_soft(pix_t *fb, unsigned int framenum, tod_t *time, int param)
{
	int i;
	int h, m, s;

	// 9 looks good/smooth, but is a bit too vague for time-telling:
	const int blobwidth = 7;

	// Convert the time into angular quantities from TDC:
	s = time->sec*64 + time->subsec;	// 60*64
	m = time->min*64 + (s/60);	       	// 60*64
	h = (5*time->hour*64) + (5*m/60);      	// 60*64

	for (i = 0; i < 60; i++) {
		fb[i].r = 0;
		fb[i].g = 0;
		fb[i].b = 0;
	}

	// Start points and fractions for each of the hands.  The mid-point of
	// the blob is the 'hand' position!
	int st_s = (s/64) - (blobwidth/2);
	int fr_s = s % 64;
	int st_m = (m/64) - (blobwidth/2);
	int fr_m = m % 64;
	int st_h = (h/64) - (blobwidth/2);
	int fr_h = h % 64;

	// Offset '0' always outputs brightness value 0, so don't need to start
	// at index 0.
	for (i = 1; i < blobwidth; i++) {
		int br_h, br_m, br_s;
		// 270 to 270
		int theta = (SINTAB_ENTRIES*3/4) + ((i*(SINTAB_ENTRIES))/(blobwidth-1));
		int t_h, t_m, t_s;

		t_h = theta - (fr_h*SINTAB_ENTRIES)/((blobwidth-1)*64);
		t_m = theta - (fr_m*SINTAB_ENTRIES)/((blobwidth-1)*64);
		t_s = theta - (fr_s*SINTAB_ENTRIES)/((blobwidth-1)*64);

		br_h = 128+((128*SIN(t_h))>>SINTAB_SHIFT);
		br_m = 128+((128*SIN(t_m))>>SINTAB_SHIFT);
		br_s = 128+((128*SIN(t_s))>>SINTAB_SHIFT);

		int p_h = (st_h + i) % 60;
		if (p_h < 0)
			p_h += 60;

		int p_m = (st_m + i) % 60;
		if (p_m < 0)
			p_m += 60;

		int p_s = (st_s + i) % 60;
		if (p_s < 0)
			p_s += 60;

		fb[p_h].r = br_h;
		fb[p_m].g = br_m;
		fb[p_s].b = br_s;
	}

	if (param == 1) {
		d_ticks(fb, 32);
	}
}

void d_minimale(pix_t *fb, unsigned int framenum, tod_t *time, int param)
{
	int i;

	for (i = 0; i < 60; i++) {
		int bri = 0;

		if (i == 0) {
			fb[0].r	= 255;
			fb[0].g	= 0;
			fb[0].b	= 0;
		} else {
			if (i <= (((time->hour * 60) + time->min)/12)) {
				if (i % 5 == 0)
					bri = 96;
				else
					bri = 255;
			} else {
				if (i % 5 == 0)
					bri = 4;
			}

			fb[i].r = bri;
			fb[i].g = bri;
			fb[i].b = bri;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

static struct dvar {
	dispfunc f;
	int param;
} disp_variants[] = {
	{ d_pie,		0 },
	{ d_pie,		1 },
	{ d_pie,		2 },
	{ d_pie,		3 },
	{ d_simple_soft,       	0 },
	{ d_simple_soft,       	1 },
	{ d_simple_soft,	2 },
	{ d_simple_soft,	3 },
	{ d_blob_soft,		0 },
	{ d_blob_soft,		1 },
	{ d_minimale,		0 },
};

static const unsigned int disp_len = sizeof(disp_variants) / sizeof(struct dvar);

void	display_init(void)
{
#ifdef SIM
	char *c = getenv("START_FX");
	if (c)
		current_disp = atoi(c);
#else
	if (RTC_ReadBackupRegister(RTC_BKP_DR0) == 0xdeadbeef) {
		current_disp = RTC_ReadBackupRegister(RTC_BKP_DR1);
	}
#endif
}

void 	display_draw(pix_t *fb, unsigned int framenum, tod_t *time)
{
	disp_variants[current_disp].f(fb, framenum, time, disp_variants[current_disp].param);
}

void	display_next(void)
{
	if (++current_disp >= disp_len)
		current_disp = 0;
#ifndef SIM
	RTC_WriteBackupRegister(RTC_BKP_DR1, current_disp);
#endif
}

void	display_prev(void)
{
	if (current_disp == 0)
		current_disp = disp_len-1;
	else
		current_disp--;
#ifndef SIM
	RTC_WriteBackupRegister(RTC_BKP_DR1, current_disp);
#endif
}

// Used for time-set faces
void 	display_drawspecial(pix_t *fb,
			    DispType t,
			    unsigned int param_a,
			    unsigned int param_b)
{
	// t is DS_HR or DS_MIN:
	//  param_a is a blanking flag (1 = display, 0 = blank)
	//  param_b is the time (0-11 or 0-59)
	// t is DS_BR_H/L:
	//  param_a is blanking as above
	//  param_b is the level (0-255)
	int i;
	int blank = !param_a;

	// Draw on a black background with ticks:
	for (i = 0; i < 60; i++) {
		int j = ((i % 5) != 0) || blank ? 0 : 4;

		fb[i].r = j;
		fb[i].g = j;
		fb[i].b = j;
	}

	if (blank)
		return;

	switch (t) {
	case DS_HR:
	case DS_MIN: {
		// Render an arc, depending on colour:
		int top = (t == DS_HR) ? param_b * 5 : param_b;

		for (i = 0; i <= top; i++) {
			if (t == DS_HR)
				fb[i].r = 255;
			else if (t == DS_MIN)
				fb[i].g = 255;
		}
	} break;
	case DS_BR_H: {
		// Starts at top and goes downwards as numbers decrease
		int nr = (255-param_b)*30/256;
		for (i = 0; i <= nr; i++) { // Rounds down on divide...
			fb[i].r = 255;
			fb[i].g = 128;
			if (i > 0) {
				fb[60-i].r = 255;
				fb[60-i].g = 128;
			}
		}
	} break;
	case DS_BR_L: {
		// Starts at bottom & goes upwards as numbers increase
		int nr = param_b*30/256;
		for (i = 0; i <= nr; i++) { // Rounds down on divide...
			fb[30-i].r = 255;
			fb[30-i].g = 128;
			if (i < 30) {
				fb[30+i].r = 255;
				fb[30+i].g = 128;
			}
		}
	} break;
	}
}
