/*
 * FIG : Facility for Interactive Generation of figures
 * Copyright (c) 1985-1988 by Supoj Sutanthavibul
 * Parts Copyright (c) 1989-2015 by Brian V. Smith
 * Parts Copyright (c) 1991 by Paul King
 * Parts Copyright (c) 2016-2020 by Thomas Loimer
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

/*
 * u_colors.h
 * Author: Thomas Loimer, 2018
 *
 *	Provide variables and routines related to color.
 *
 */


#ifndef U_COLORS_H
#define U_COLORS_H

#if defined HAVE_CONFIG_H && !defined VERSION
#include "config.h"			/* restrict */
#endif

#include <X11/Intrinsic.h>		/* Boolean */
#include <X11/Xft/Xft.h>

/* Color definition */
#define	Color		int
#define NUM_STD_COLS	32	/* Number of standard colors. */
#define MAX_USR_COLS	512	/* Maximum number of user-defined colors. */
#define	MAX_COLORMAP_SIZE	MAX_USR_COLS	/* For picture files. */

/* indices of special colors */
#define CANVAS_BG	(-7)	/* canvas background color */
#define SPECIAL_COLS	7	/* number of special colors */
#define DARK_GRAY	(-6)	/* color to greek small text */
#define MED_GRAY	(-5)	/* color to gray out inactive layers */
#define LT_GRAY		(-4)
#define TRANSP_BACKGROUND (-3)	/* use background of figure as transp color */
#define TRANSP_NONE	(-2)	/* no transp color */
#define COLOR_NONE	(-2)	/* no background color (exporting) */
#define DEFAULT		(-1)

#define BLACK		0
#define BLUE		1
#define GREEN		2
#define CYAN		3
#define RED		4
#define MAGENTA		5
#define YELLOW		6
#define WHITE		7
#define GREEN4		12

#define OPAQUE		0xffff		/* opaque alpha mask */

struct Cmap {
	unsigned short red, green, blue;
	unsigned long pixel;
};
typedef struct {
	char		*name;
	char		*shrt;
	unsigned short	red;
	unsigned short	green;
	unsigned short	blue;
} fig_color;

/* Globals */
extern Boolean		all_colors_available;
extern Boolean		colorUsed[MAX_USR_COLS];
extern Boolean		colorFree[MAX_USR_COLS];
extern Boolean		n_colorFree[MAX_USR_COLS];
extern int		num_usr_cols, n_num_usr_cols;
extern unsigned long	axis_lines_color;
extern unsigned long	pageborder_color;
extern unsigned long	grid_color;
extern XftColor		*user_color;
extern XftColor		*xftcolor;
extern XftColor		n_user_colors[MAX_USR_COLS];
extern fig_color	colorNames[NUM_STD_COLS + 1];

/* For GIF/XPM images */
/* number of colors we want to use for GIF/XPM images */
extern int		avail_image_cols;

extern void		check_colors(void);
extern void		xtoxftcolor(XftColor *out, const XColor *restrict in);
extern void		setcolor_fromXColor(int c, const XColor *restrict in);
extern unsigned long	getpixel(int color);
extern unsigned short	getred(int color), getgreen(int color), getblue(int c);

#endif /* U_COLORS_H */
