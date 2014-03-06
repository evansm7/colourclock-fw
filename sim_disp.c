/* Copyright (c) 2014 Matt Evans
 *
 * sim_disp:  Simulation build, SDL simulation of LEDs.
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
#include <SDL.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#endif

SDL_Surface* screen = NULL;
SDL_Surface* led = NULL;
SDL_Window *window = NULL;

static const int s_width = 1100;
static const int s_height = 800;

static const int l_size = 30;
static int radius;

static int sim_disp_evt = 0;

int sim_disp_event(void)
{
	int i = sim_disp_evt;
	sim_disp_evt = 0;
	return i;
}

void sim_disp_init(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_VIDEO);

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 6);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 6);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

	window = SDL_CreateWindow("Thing", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
				  s_width, s_height, SDL_WINDOW_OPENGL);
	if (!window) {
		fprintf(stderr, "Can't create window\n");
		exit(1);
	}
	SDL_GLContext *glc = SDL_GL_CreateContext(window);

	// vsync lock:
	SDL_GL_SetSwapInterval(1);

	glShadeModel( GL_SMOOTH );
	glCullFace( GL_BACK );
	glFrontFace( GL_CCW );
	glEnable( GL_CULL_FACE );
	glClearColor(0, 0, 0, 0);
	glEnable(GL_MULTISAMPLE);

	glViewport(0, 0, s_width, s_height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, s_width, 0, s_height, -10, 10);

	radius = s_width < s_height ?
		(s_width/2 - l_size*1.5) : (s_height/2 - l_size*1.5);
}

void sim_disp_close(void)
{
	SDL_Quit();
}

static void quit(void)
{
	printf("Bye\n");
	exit(0);
}

void sim_disp_sync(pix_t *fb)
{
	int i;
	SDL_Event e;
	static int shifted = 0;

	while(SDL_PollEvent(&e)) {
		switch(e.type) {
		case SDL_KEYDOWN:
			switch (e.key.keysym.sym) {
			case SDLK_ESCAPE:
				quit();
				break;
			case SDLK_DOWN:
				sim_disp_evt = 1 | (shifted ? 0x80 : 0);
				break;
			case SDLK_LEFT:
				sim_disp_evt = 2 | (shifted ? 0x80 : 0);
				break;
			case SDLK_RIGHT:
				sim_disp_evt = 4 | (shifted ? 0x80 : 0);
				break;
			case SDLK_RSHIFT:
				shifted = 1;
				break;
			}
			break;
		case SDL_KEYUP:
			switch (e.key.keysym.sym) {
			case SDLK_RSHIFT:
				shifted = 0;
				break;
			}
			break;
		case SDL_QUIT:
			quit();
		}
	}


	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity( );

	glTranslatef(s_width/2, s_height/2, 0);

	for (i = 0; i < 60; i++) {
		const int grey = 0;
		unsigned int r, g, b;

		// Simulate 6-BPC output:
		r = fb[i].r & 0xfc;
		g = fb[i].g & 0xfc;
		b = fb[i].b & 0xfc;

		glPushMatrix();

		glRotatef(90+i*6, 0, 0, -1);

		glTranslatef(-radius, 0, 0);

		/* Send our triangle data to the pipeline. */
		glBegin( GL_QUADS );
		glColor4ub(fb[i].r, fb[i].g, fb[i].b, 255 );
		glVertex3i(-l_size, -(l_size/2), 0 );
		glColor4ub(grey, grey, grey, 255 );
		glVertex3i(l_size, -(l_size/2), 0);
		glColor4ub(grey, grey, grey, 255 );
		glVertex3i(l_size, (l_size/2), 0);
		glColor4ub(fb[i].r, fb[i].g, fb[i].b, 255 );
		glVertex3i(-l_size, (l_size/2), 0);
		glEnd();
		glPopMatrix();
	}

	SDL_GL_SwapWindow(window); 	// Syncs to VSYNC
}
