/*
 * FIG : Facility for Interactive Generation of figures
 * Copyright (c) 1985-1988 by Supoj Sutanthavibul
 * Parts Copyright (c) 1989-2015 by Brian V. Smith
 * Parts Copyright (c) 1991 by Paul King
 * Parts Copyright (c) 2016-2023 by Thomas Loimer
 *
 * Any party obtaining a copy of these files is granted, free of charge, a
 * full and unrestricted irrevocable, world-wide, paid up, royalty-free,
 * nonexclusive right and license to deal in this software and documentation
 * files (the "Software"), including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense and/or sell copies of
 * the Software, and to permit persons who receive copies from any such
 * party to do so, with the only requirement being that the above copyright
 * and this permission notice remain intact.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "w_rottext.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "resources.h"
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "xfig_math.h"


/* Debugging macros */

#define DEBUG_PRINT1(a) /* if (appres.DEBUG) printf (a) */

/* EXPORTS */

int		XRotDrawString(Display *dpy, XFontStruct *font,
			Drawable drawable, GC gc, int x, int y, char *str);
int		XRotDrawImageString(Display *dpy, XFontStruct *font,
			Drawable drawable, GC gc, int x, int y, char *str);

/* LOCALS */

static char	*my_strtok(char *str1, char *str2);
static int	XRotDrawHorizontalString(Display *dpy, XFontStruct *font,
			Drawable drawable, GC gc, int x, int y, char *text,
			int align, int bg);


/**************************************************************************/
/* Routine to replace `strtok' : this one returns a zero length string if */
/* it encounters two consecutive delimiters                               */
/**************************************************************************/

static char
*my_strtok(char *str1, char *str2)
{
	char		*ret;
	int		stop;
	size_t		i, j;
	static size_t	start, len;
	static char	*stext;

	if (str2==NULL)
		return NULL;

	/* initialise if str1 not NULL */
	if (str1!=NULL) {
		start=0;
		stext=str1;
		len = strlen(str1);
	}

	/* run out of tokens ? */
	if (start >= len)
		return NULL;

	/* loop through characters */
	for(i = start; i < len; i++) {
		/* loop through delimiters */
		stop = 0;
		for(j = 0; j < strlen(str2); j++)
			if (stext[i] == str2[j])
				stop = 1;

		if (stop)
			break;
	}

	stext[i]='\0';

	ret = stext + start;

	start = i + 1;

	return ret;
}


/* ---------------------------------------------------------------------- */


/**************************************************************************/
/*  A front end to XRotDrawHorizontalString:                              */
/*      -no alignment, no background                                      */
/**************************************************************************/

int
XRotDrawString(Display *dpy, XFontStruct *font, Drawable drawable, GC gc,
		int x, int y, char *str)
{
	return XRotDrawHorizontalString(dpy, font, drawable, gc,
					x, y, str, NONE, 0);
}


/* ---------------------------------------------------------------------- */


/**************************************************************************/
/*  A front end to XRotDrawHorizontalString:                              */
/*      -no alignment, paints background                                  */
/**************************************************************************/

int
XRotDrawImageString(Display *dpy, XFontStruct *font, Drawable drawable, GC gc,
		int x, int y, char *str)
{
	return XRotDrawHorizontalString(dpy, font, drawable, gc,
					x, y, str, NONE, 1);
}


/* ---------------------------------------------------------------------- */


/**************************************************************************/
/*  Draw a horizontal string in a quick fashion                           */
/**************************************************************************/

static int
XRotDrawHorizontalString(Display *dpy, XFontStruct *font, Drawable drawable,
		GC gc, int x, int y, char *text, int align, int bg)
{
	GC		my_gc;
	size_t		nl = 1, i;
	int		height;
	int		xp, yp;
	char		*str1, *str2, *str3;
	char		*str2_a="\0", *str2_b = "\n\0";
	int		dir, asc, desc;
	XCharStruct	overall;

	if (text == NULL || *text=='\0') {
		DEBUG_PRINT1("Empty string, ignoring\n");
		return 0;
	}

	/* this gc has similar properties to the user's gc (including stipple) */
	my_gc=XCreateGC(dpy, drawable, (unsigned long) 0, 0);
	XCopyGC(dpy, gc,
			GCForeground | GCBackground | GCFunction | GCStipple|
			GCFillStyle | GCClipMask | GCClipXOrigin |
			GCClipYOrigin| GCSubwindowMode | GCTileStipXOrigin |
			GCTileStipYOrigin | GCPlaneMask,
			my_gc);
	XSetFont(dpy, my_gc, font->fid);

	/* count number of sections in string */
	if (align!=NONE)
		for(i = 0; i < strlen(text) - 1; i++)
			if (text[i] == '\n')
				nl++;

	/* ignore newline characters if not doing alignment */
	if (align == NONE)
		str2 = str2_a;
	else
		str2 = str2_b;

	/* overall font height */
	height = font->ascent + font->descent;

	/* y position */
	if (align==TLEFT || align==TCENTRE || align==TRIGHT)
		yp=y+font->ascent;
	else if (align==MLEFT || align==MCENTRE || align==MRIGHT)
		yp=y-nl*height/2+font->ascent;
	else if (align==BLEFT || align==BCENTRE || align==BRIGHT)
		yp=y-nl*height+font->ascent;
	else
		yp=y;

	str1=strdup(text);
	if (str1==NULL)
		return 1;

	str3=my_strtok(str1, str2);

	/* loop through each section in the string */
	do {
		XTextExtents(font, str3, strlen(str3), &dir, &asc, &desc,
				&overall);

		/* where to draw section in x ? */
		if (align==TLEFT || align==MLEFT || align==BLEFT || align==NONE)
			xp=x;
		else if (align==TCENTRE || align==MCENTRE || align==BCENTRE)
			xp=x-overall.rbearing/2;
		else
			xp=x-overall.rbearing;

		/* draw string onto bitmap */
		if (!bg)
			XDrawString(dpy, drawable, my_gc, xp, yp, str3,
					strlen(str3));
		else
			XDrawImageString(dpy, drawable, my_gc, xp, yp, str3,
					strlen(str3));

		/* move to next line */
		yp += height;

		str3 = my_strtok((char *)NULL, str2);
	} while(str3 != NULL);

	free(str1);
	XFreeGC(dpy, my_gc);

	return 0;
}
