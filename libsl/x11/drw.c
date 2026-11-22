/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>

#include "drw.h"
#include "util.h"

Drw *
drw_create(Display *dpy, int screen, Window root, unsigned int w, unsigned int h)
{
	Drw *drw;

	drw = ecalloc(1, sizeof(Drw));
	drw->dpy = dpy;
	drw->screen = screen;
	drw->root = root;
	drw->w = w;
	drw->h = h;
	drw->drawable = XCreatePixmap(dpy, root, w, h, DefaultDepth(dpy, screen));
	drw->gc = XCreateGC(dpy, root, 0, NULL);
	XSetLineAttributes(dpy, drw->gc, 1, LineSolid, CapButt, JoinMiter);

	return drw;
}

void
drw_resize(Drw *drw, unsigned int w, unsigned int h)
{
	drw->w = w;
	drw->h = h;
	if (drw->drawable)
		XFreePixmap(drw->dpy, drw->drawable);
	drw->drawable = XCreatePixmap(drw->dpy, drw->root, w, h, DefaultDepth(drw->dpy, drw->screen));
}

void
drw_free(Drw *drw)
{
	XFreePixmap(drw->dpy, drw->drawable);
	XFreeGC(drw->dpy, drw->gc);
	free(drw);
}

static Fnt *
xfont_create(Display *dpy, const char *fontname)
{
	Fnt *font;
	XFontStruct **xfonts;
	char **font_names;
	char *def, **missing;
	int n;

	font = ecalloc(1, sizeof(Fnt));
	font->dpy = dpy;
	font->set = XCreateFontSet(dpy, fontname, &missing, &n, &def);
	if (missing) {
		while (n--)
			fprintf(stderr, "drw: missing fontset: %s\n", missing[n]);
		XFreeStringList(missing);
	}
	if (font->set) {
		XExtentsOfFontSet(font->set);
		n = XFontsOfFontSet(font->set, &xfonts, &font_names);
		while (n--) {
			font->ascent = MAX(font->ascent, (*xfonts)->ascent);
			font->descent = MAX(font->descent,(*xfonts)->descent);
			xfonts++;
		}
	} else {
		if (!(font->xfont = XLoadQueryFont(dpy, fontname))
		&& !(font->xfont = XLoadQueryFont(dpy, "fixed")))
			die("error, cannot load font: '%s'\n", fontname);
		font->ascent = font->xfont->ascent;
		font->descent = font->xfont->descent;
	}
	font->h = font->ascent + font->descent;

	return font;
}

void
xfont_free(Display *dpy, Fnt *font)
{
	if (font->set)
		XFreeFontSet(dpy, font->set);
	else
		XFreeFont(dpy, font->xfont);
	free(font);
}

Fnt *
drw_fontset_create(Drw* drw, const char *fonts[], size_t fontcount)
{
	/* just create using first font */
	/* TODO: more? */
	drw->fonts = xfont_create(drw->dpy, fonts[0]);
	return drw->fonts;
}

void
drw_fontset_free(Fnt *font)
{
	xfont_free(font->dpy, font);
}

void
drw_clr_create(Drw *drw, Clr *dest, const char *clrname)
{
	Colormap cmap;
	XColor ecolor;

	cmap = DefaultColormap(drw->dpy, drw->screen);
	if (!XAllocNamedColor(drw->dpy, cmap, clrname, dest, &ecolor))
		die("error, cannot allocate color '%s'\n", clrname);
}

/* Wrapper to create color schemes. The caller has to call free(3) on the
 * returned color scheme when done using it. */
Clr *
drw_scm_create(Drw *drw, const char *clrnames[], size_t clrcount)
{
	size_t i;
	Clr *ret;

	ret = ecalloc(clrcount, sizeof(Clr));
	for (i = 0; i < clrcount; i++)
		drw_clr_create(drw, &ret[i], clrnames[i]);

	return ret;
}

void
drw_clr_free(Drw *drw, Clr *c)
{
	if (!drw || !c)
		return;

	/* c is typedef XColor Clr */
	/* TODO: free XColor? */
}

void
drw_scm_free(Drw *drw, Clr *scm, size_t clrcount)
{
	size_t i;

	if (!drw || !scm)
		return;

	for (i = 0; i < clrcount; i++)
		drw_clr_free(drw, &scm[i]);
	free(scm);
}

void
drw_setfontset(Drw *drw, Fnt *set)
{
	drw->fonts = set;
}

void
drw_setscheme(Drw *drw, Clr *scheme)
{
	drw->scheme = scheme;
}

void
drw_rect(Drw *drw, int x, int y, unsigned int w, unsigned int h, int filled,
	int invert)
{
	XSetForeground(drw->dpy, drw->gc, drw->scheme[invert ? ColBg : ColFg].pixel);
	if (filled)
		XFillRectangle(drw->dpy, drw->drawable, drw->gc, x, y, w + 1, h + 1);
	else
		XDrawRectangle(drw->dpy, drw->drawable, drw->gc, x, y, w, h);
}

int
drw_text(Drw *drw, int x, int y, unsigned int w, unsigned int h, unsigned int lpad,
	const char *text, int invert)
{
	char buf[256];
	int i, tx, ty, th, len, olen, render = x || y || w || h;
	unsigned int texw, texh;

	if (!render) {
		w = ~w;
	} else {
		XSetForeground(drw->dpy, drw->gc, drw->scheme[invert ? ColFg : ColBg].pixel);
		XFillRectangle(drw->dpy, drw->drawable, drw->gc, x, y, w, h);
		x += lpad;
		w -= lpad;
	}

	olen = strlen(text);
	drw_font_getexts(drw->fonts, text, olen, &texw, &texh);
	th = drw->fonts->ascent + drw->fonts->descent;
	ty = y + (h / 2) - (th / 2) + drw->fonts->ascent;
	tx = x;
	/* shorten text if necessary */
	for (len = MIN(olen, sizeof buf); len && texw > w; len--)
		drw_font_getexts(drw->fonts, text, len, &texw, &texh);
	if (!len)
		return x;
	memcpy(buf, text, len);
	if (len < olen)
		for (i = len; i && i > len - 3; buf[--i] = '.')
			;
	XSetForeground(drw->dpy, drw->gc, invert ? drw->scheme[ColBg].pixel : drw->scheme[ColFg].pixel);
	if (drw->fonts->set)
		XmbDrawString(drw->dpy, drw->drawable, drw->fonts->set, drw->gc, tx, ty, buf, len);
	else
		XDrawString(drw->dpy, drw->drawable, drw->gc, tx, ty, buf, len);

	x += texw;
	w -= texw;

	return x + (render ? w : 0);
}

void
drw_map(Drw *drw, Window win, int x, int y, unsigned int w, unsigned int h)
{
	XCopyArea(drw->dpy, drw->drawable, win, drw->gc, x, y, w, h, x, y);
	XSync(drw->dpy, False);
}

static unsigned int
drw_font_getexts_width(Fnt *font, const char *text, unsigned int len)
{
	unsigned int w;

	drw_font_getexts(font, text, len, &w, NULL);

	return w;
}

unsigned int
drw_fontset_getwidth(Drw *drw, const char *text)
{
	return drw_font_getexts_width(drw->fonts, text, strlen(text));
}

unsigned int
drw_fontset_getwidth_clamp(Drw *drw, const char *text, unsigned int n)
{
	unsigned int tmp = 0;
	if (drw && drw->fonts && text && n)
		tmp = drw_text(drw, 0, 0, 0, 0, 0, text, n);
	return MIN(n, tmp);
}

void
drw_font_getexts(Fnt *font, const char *text, unsigned int len, unsigned int *w, unsigned int *h)
{
	XRectangle r;

	if (font->set) {
		XmbTextExtents(font->set, text, len, NULL, &r);
		if (w)
			*w = r.width;
		if (h)
			*h = r.height;
	} else {
		if (w)
			*w = XTextWidth(font->xfont, text, len);
		if (h)
			*h = font->ascent + font->descent;
	}
}

Cur *
drw_cur_create(Drw *drw, int shape)
{
	Cur *cur;

	cur = ecalloc(1, sizeof(Cur));
	cur->cursor = XCreateFontCursor(drw->dpy, shape);

	return cur;
}

void
drw_cur_free(Drw *drw, Cur *cursor)
{
	XFreeCursor(drw->dpy, cursor->cursor);
	free(cursor);
}
