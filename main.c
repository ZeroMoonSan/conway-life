#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdbool.h>
#include <X11/Xlib.h>
#include <x11/drw.h>
#include <util.h>
#include <time.h>
#include <string.h>

#include "config.h"

static void setup();
static void update();
static void draw();
static void sleep(long ms);

unsigned int cell;

Display *dpy;
Window win;
Drw *drw;
XEvent ev;

bool newBuf[MW*MH] = {0};
bool preBuf[MW*MH];

int main()
{
	cell = MIN(W, H) / MAX(MW, MH);
	dpy = XOpenDisplay(NULL);
	if (!dpy)
		die("cannot open display");
	int screen = DefaultScreen(dpy);
	win = XCreateSimpleWindow(
			dpy, RootWindow(dpy, screen),
			0, 0, W, H, 0, 0, 0);

	XSelectInput(dpy, win, KeyPressMask | ButtonPressMask | ExposureMask);
	XMapWindow(dpy, win);

	drw = drw_create(dpy, screen, win, W, H);

	Clr *scheme = drw_scm_create(drw, colors, 2);
	drw_setscheme(drw, scheme);

	setup();

	while (True) {
		while (XPending(dpy)) {
			XNextEvent(dpy, &ev);
			if (ev.type == Expose) { draw(); }
		}
		update();
		draw();
		sleep(10);
	}
	return 0;
}

void setup()
{
	srand((unsigned)time(NULL));

	for (int i = 0; i < MH; i++) {
		for (int j = 0; j < MW; j++) {
			newBuf[i*MW + j] = rand() % 2 == 0;	
		}
	}
}

void update()
{
	memcpy(preBuf, newBuf, sizeof(newBuf));
	for (int i = 1; i < MH-1; i++) {
		for (int j = 1; j < MW-1; j++) {
			int nbrs = 0;
			for (int d = 0; d < LENGTH(offs); d++)
				if (preBuf[i*MW + j + offs[d]] > 0) nbrs++;
			if (nbrs > 2) newBuf[i*MW + j] = 1;
			if (nbrs < 2 || nbrs > 3) newBuf[i*MW + j] = 0;
		}
	}
}

void draw()
{
	drw_rect(drw, 0, 0, W, H, 1, 1); // bg

	for (int i = 0; i < MH; i++) {
		for (int j = 0; j < MW; j++) {
			if (newBuf[i*MW + j] > 0)
				drw_rect(drw, j*cell, i*cell, cell, cell, 1, 0);
		}
	}
	drw_map(drw, win, 0, 0, W, H);
}

void sleep(long ms)
{
	struct timespec ts = {
		.tv_sec  = ms / 1000,
		.tv_nsec = (ms % 1000) * 1000000L,
	};
	nanosleep(&ts, NULL);
}
