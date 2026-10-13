/*
 * FIG : Facility for Interactive Generation of figures
 * Copyright (c) 1985-1988 by Supoj Sutanthavibul
 * Parts Copyright (c) 1989-2015 by Brian V. Smith
 * Parts Copyright (c) 1991 by Paul King
 * Parts Copyright (c) 2016-2024 by Thomas Loimer
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

#include <stdio.h>
#include <X11/Xlib.h>			/* XColor, Status; includes X11/X.h */
#include <X11/Xft/Xft.h>

#include "resources.h"			/* tool_d, tool_v,.., appres */
#include "u_colors.h"
#include "w_color.h"			/* switch_colormap() */

#define FIRST_GRAY	DARK_GRAY
#define LAST_GRAY	LT_GRAY

Boolean		all_colors_available;

/* variables used for color administration, e.g., when opening a new file */
Boolean		colorUsed[MAX_USR_COLS];
Boolean		colorFree[MAX_USR_COLS];
Boolean		n_colorFree[MAX_USR_COLS];
int		num_usr_cols = 0;
int		n_num_usr_cols;
XftColor	n_user_colors[MAX_USR_COLS];

static XftColor	color_storage[SPECIAL_COLS + NUM_STD_COLS + MAX_USR_COLS];
/* Convenience pointers into interesting positions of color_storage. */
XftColor	*xftcolor = color_storage + SPECIAL_COLS;
XftColor	*user_color = color_storage + SPECIAL_COLS + NUM_STD_COLS;
unsigned long	pageborder_color;
unsigned long	axis_lines_color;
unsigned long	grid_color;


/* Number of colors we want to use for pictures. This will be determined
   when the first picture is used. We will take
   min(number_of_free_colorcells, 100, appres.maximagecolors) */
int		avail_image_cols = -1;

fig_color colorNames[] = {
	{"Default",	"Default",	0,	0,	0},
	{"Black",	"Blk",		0,	0,	0},
	{"Blue",	"Blu",		0x0000, 0x0000, 0xffff},
	{"Green",	"Grn",		0x0000, 0xffff, 0x0000},
	{"Cyan",	"Cyn",		0x0000, 0xffff, 0xffff},
	{"Red",		"Red",		0xffff, 0x0000, 0x0000},
	{"Magenta",	"Mag",		0xffff, 0x0000, 0xffff},
	{"Yellow",	"Yel",		0xffff, 0xffff, 0x0000},
	{"White",	"Wht",		0xffff, 0xffff, 0xffff},
	{"Blue4",	"Bl4",		0x0000, 0x0000, 0x9000},
	{"Blue3",	"Bl3",		0x0000, 0x0000, 0xb000},
	{"Blue2",	"Bl2",		0x0000, 0x0000, 0xd000},
	{"LtBlue",	"LBl",		0x8700, 0xce00, 0xff00},
	{"Green4",	"Gr4",		0x0000, 0x9000, 0x0000},
	{"Green3",	"Gr3",		0x0000, 0xb000, 0x0000},
	{"Green2",	"Gr2",		0x0000, 0xd000, 0x0000},
	{"Cyan4",	"Cn4",		0x0000, 0x9000, 0x9000},
	{"Cyan3",	"Cn3",		0x0000, 0xb000, 0xb000},
	{"Cyan2",	"Cn2",		0x0000, 0xd000, 0xd000},
	{"Red4",	"Rd4",		0x9000, 0x0000, 0x0000},
	{"Red3",	"Rd3",		0xb000, 0x0000, 0x0000},
	{"Red2",	"Rd2",		0xd000, 0x0000, 0x0000},
	{"Magenta4",	"Mg4",		0x9000, 0x0000, 0x9000},
	{"Magenta3",	"Mg3",		0xb000, 0x0000, 0xb000},
	{"Magenta2",	"Mg2",		0xd000, 0x0000, 0xd000},
	{"Brown4",	"Br4",		0x8000, 0x3000, 0x0000},
	{"Brown3",	"Br3",		0xa000, 0x4000, 0x0000},
	{"Brown2",	"Br2",		0xc000, 0x6000, 0x0000},
	{"Pink4",	"Pk4",		0xff00, 0x8000, 0x8000},
	{"Pink3",	"Pk3",		0xff00, 0xa000, 0xa000},
	{"Pink2",	"Pk2",		0xff00, 0xc000, 0xc000},
	{"Pink",	"Pnk",		0xff00, 0xe000, 0xe000},
	{"Gold",	"Gld",		0xff00, 0xd700, 0x0000}
};


/*
 * Set XftColor *out to the color values given in XColor *in.
 */
void
xtoxftcolor(XftColor *out, const XColor *restrict in)
{
	out->pixel = in->pixel;
	out->color.red = in->red;
	out->color.green = in->green;
	out->color.blue = in->blue;
	out->color.alpha = OPAQUE;
}


/* For TrueColor visuals: Given a fig_color, return the XftColor. */
static void
write_xftcolor_true(const fig_color *restrict in, int c)
{
	XRenderColor	buf;

	buf.red = in->red;
	buf.green = in->green;
	buf.blue = in->blue;
	buf.alpha = OPAQUE;

	XftColorAllocValue(tool_d, tool_v, tool_cm, &buf, &xftcolor[c]);
}

/*
 * For non-TrueColor visuals, given a fig_color, return the XftColor.
 *
 * For non-TrueColor visuals, I believe, XftColorAllocValue() returns the
 * requested color, which may be different from the really allocated color, see
 * https://gitlab.freedesktop.org/xorg/lib/libxft/issues/7 .
 * Therefore, call XallocColor() and use that result.
 */
static Status
write_xftcolor_nontrue(const fig_color *restrict in, int c)
{
	XColor	buf;
	Status	status;

	buf.red = in->red;
	buf.green = in->green;
	buf.blue = in->blue;
	/* buf.flags = DoRed | DoGreen | DoBlue; not used by XAllocColor() */
	if ((status = XAllocColor(tool_d, tool_cm, &buf))) {
		xtoxftcolor(&xftcolor[c], &buf);
		xftcolor[c].color.alpha = OPAQUE;
	}
	return status;
}

/*
 * Set xftcolor[c] to the color specified in XColor *in.
 */
void
setcolor_fromXColor(int c, const XColor *restrict in)
{
	xftcolor[c].pixel = in->pixel;
	xftcolor[c].color.red = in->red;
	xftcolor[c].color.green = in->green;
	xftcolor[c].color.blue = in->blue;
	xftcolor[c].color.alpha = OPAQUE;
}

/*
 * Allocate standard colors, three gray colors, and set the pageborder,
 * axislines and grid color pixel. The latter three might be set from resources.
 * Provide for three cases, (i) TrueColor, (ii) black and white and
 * (iii) legacy XColors, with fallback values if not all colors can
 * be allocated.
 */
void
check_colors(void)
{
	int		i;
	XColor		x_color;
	fig_color	grays[LAST_GRAY - FIRST_GRAY + 1] = {
				{"", "", 166<<8, 166<<8, 166<<8},  /* gray65 */
				{"", "", 204<<8, 204<<8, 204<<8},  /* gray80 */
				{"", "", 229<<8, 229<<8, 229<<8}   /* gray90 */
			};

	/* initialize user color cells */
	for (i = 0; i < MAX_USR_COLS; ++i) {
		colorFree[i] = True;
		n_colorFree[i] = True;
		num_usr_cols = 0;
	}

	/* if monochrome resource is set, only allocate black and white */
	/* appres.monochrome and tool_cells == 2 is unnecessary, since
	   in that case init_settings() sets all_colors_available = false */
	if (!all_colors_available || appres.monochrome || tool_cells == 2) {

		/* set all colors to black, except white */
		for (i = 0; i < NUM_STD_COLS; ++i)
			if (i == WHITE)
				write_xftcolor_true(&colorNames[i + 1], i);
			else
				write_xftcolor_true(&colorNames[BLACK + 1], i);

		/* set all grays to white */
		for (i = FIRST_GRAY; i <= LAST_GRAY; ++i)
			xftcolor[i] = xftcolor[WHITE];

		pageborder_color = axis_lines_color = grid_color =
								getpixel(BLACK);

		return;
	}

	/*
	 * XftColorAllocValue() seems to return the requested color, not the
	 * color actually allocated by the server (see issue 7 at
	 * https://gitlab...) Therefore, use
	 * XftColorAllocValue() for TrueColor visuals only, otherwise use
	 * XAllocColor etc.
	 */
	if (tool_vclass == TrueColor) {

		/* Standard colors */
		for (i = 0; i < NUM_STD_COLS; ++i) {
			write_xftcolor_true(&colorNames[i + 1], i);
		}
		/* Gray colors */
		for (i = FIRST_GRAY; i <= LAST_GRAY; ++i)
			write_xftcolor_true(&grays[i - FIRST_GRAY], i);

	} else { /* !TrueColor */

		/* Allocate the standard colors, otherwise set to black. */
		for (i = 0; i < NUM_STD_COLS; ++i) {
			if (i == WHITE) {
				write_xftcolor_nontrue(&colorNames[i + 1], i);
			} else if (!all_colors_available) {
				xftcolor[i] = xftcolor[BLACK];
			} else if (!write_xftcolor_nontrue(
						&colorNames[i + 1], i)) {
				if (!switch_colormap() ||
						!write_xftcolor_nontrue(
						    &colorNames[i + 1], i)) {
#define CMAPERR	"Not enough colormap entries available for basic colors, using monochrome mode.\n"
					fprintf(stderr, CMAPERR);
					all_colors_available = False;
					xftcolor[i] = xftcolor[BLACK];
				}
			}
		}

		for (i = FIRST_GRAY; i <= LAST_GRAY; ++i) {
			if (!all_colors_available) {
				xftcolor[i] = xftcolor[WHITE];
			} else if (!write_xftcolor_nontrue(&grays[i-FIRST_GRAY],
						i)) {
				if (!switch_colormap() ||
						!write_xftcolor_nontrue(
							&grays[i-FIRST_GRAY], i)
						) {
					all_colors_available = False;
					xftcolor[i] = xftcolor[WHITE];
				}
			}
		}
	}

	/* Continue for both TrueColor and non-TrueColor visuals. */

	/* page border color */
	XParseColor(tool_d, tool_cm, appres.pageborder, &x_color);
	if (XAllocColor(tool_d, tool_cm, &x_color))
		pageborder_color = x_color.pixel;
	else
		pageborder_color = getpixel(BLACK);

	/* axis lines color */
	XParseColor(tool_d, tool_cm, appres.axislines, &x_color);
	if (XAllocColor(tool_d, tool_cm, &x_color))
		axis_lines_color = x_color.pixel;
	else
		axis_lines_color = getpixel(BLACK);

	/* parse any grid color spec */
	XParseColor(tool_d, tool_cm, appres.grid_color, &x_color);
	if (XAllocColor(tool_d, tool_cm, &x_color)) {
		grid_color = x_color.pixel;
	} else {
		fprintf(stderr, "Can't allocate color for grid \n");
		grid_color = getpixel(BLACK);
	}
}

unsigned long
getpixel(int c)
{
	return xftcolor[c].pixel;
}

unsigned short
getred(int c)
{
	return xftcolor[c].color.red;
}

unsigned short
getgreen(int c)
{
	return xftcolor[c].color.green;
}

unsigned short
getblue(int c)
{
	return xftcolor[c].color.blue;
}
