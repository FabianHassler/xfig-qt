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

#ifndef U_ERROR_H
#define U_ERROR_H

#include <X11/Intrinsic.h>	/* includes X11/Xlib.h */

extern void	emergency_quit(Boolean abortflag);
extern void	error_handler(int err_sig);
extern void	my_quit(Widget w, XEvent *event, String *params,
			Cardinal *num_params);
extern int	X_error_handler(Display *d, XErrorEvent *err_ev);

#endif
