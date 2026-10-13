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


#ifndef U_FONTS_H
#define U_FONTS_H

#if defined HAVE_CONFIG_H && !defined VERSION
#include "config.h"
#endif

#include <X11/Xlib.h>		/* includes X11/X.h */
#include <X11/Xft/Xft.h>

#include "object.h"		/* F_pos */

#define DEF_FONTSIZE		12		/* default font size in pts */
#define DEF_LATEX_FONT		0
#define PS_FONTPANE_WD		290
#define LATEX_FONTPANE_WD	112
#define PS_FONTPANE_HT		20
#define LATEX_FONTPANE_HT	20
#define NUM_FONTS		35
#define NUM_LATEX_FONTS		6
/* font number for the "nil" font (when user wants tiny text) */
#define NILL_FONT NUM_FONTS
/* to write the picture file name; keep in sync with ps_fontinfo[] */
#define FONT_COURIER	12

struct _fstruct {
    char	   *name;	/* Postscript font name */
    int		    xfontnum;	/* template for locating X fonts */
};

extern int	psfontnum(char *font);
extern int	latexfontnum(char *font);
extern void	font_baselinebbox(F_text *t, F_line *l);
extern void	font_close(struct xfig_font *xf_font);
extern void	font_drawtext(XftDraw *xftdraw, XftFont *xft_font, int fnum,
				int x, int y, const char *s, const Color c);
extern void	font_reload(F_text *t);
extern void	font_setbbox_offset(F_text *t);
extern F_pos	font_textlen(struct xfig_font *xf_font, int fnum,
				const char *string, int len);
extern F_pos	font_textoffset(struct xfig_font *xf_font, int fnum,
				const char *string, int len);
extern void	font_ascentdescent(int psflag, int fnum, int size, int *ascent,
				int *descent);
extern void	font_base(int *base_x, int *base_y, int draw_x, int draw_y,
				int align, F_pos offset);
extern void	text_origin(int *draw_x, int *draw_y, int base_x, int base_y,
				int align, F_pos offset);
extern struct xfig_font	*font_open(int psflag, int fnum, double size,
				double angle);
extern struct xfig_font	*font_xfcopy(struct xfig_font *xf_font);

extern struct _fstruct	ps_fontinfo[];
extern struct _fstruct	latex_fontinfo[];

#ifdef FREEMEM
extern void	font_closeall(void);
#endif

#endif /* U_FONTS_H */
