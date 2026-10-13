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

/* This file is part of xdir, an X-based directory browser. */

#ifndef W_BROWSE_H
#define W_BROWSE_H

#include <X11/Intrinsic.h>     /* includes X11/Xlib.h, which includes X11/X.h */

extern Widget	browse_selfile, browse_dir, browse_flist,
		browse_dlist, browse_mask;
extern char	cur_browse_dir[];
extern Boolean	browse_up;
extern void		popup_browse_panel(Widget w);

#endif
