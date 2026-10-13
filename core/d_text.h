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

#ifndef D_TEXT_H
#define D_TEXT_H

#if defined HAVE_CONFIG_H && !defined VERSION
#include "config.h"
#endif

#include <X11/Intrinsic.h>     /* includes X11/Xlib.h, which includes X11/X.h */
#include "object.h"

extern int		work_font;
extern XFontStruct	*canvas_font;
extern void	char_handler(unsigned char *c, int clen, KeySym keysym);
extern void	finish_text_input(int x, int y, int shift);

extern XIC	xim_ic;
extern Boolean	xim_active;
extern Boolean	xim_initialize(Widget w);
extern void	xim_set_ic_geometry(XIC ic, int width, int height);
extern void	kill_preedit();
extern void	text_drawing_selected (void);

#endif
