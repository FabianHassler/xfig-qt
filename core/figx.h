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

#ifndef FIGX_H
#define FIGX_H

#if defined HAVE_CONFIG_H && !defined VERSION
#include "config.h"
#endif

#ifdef XAW3D
# define XAW_V(header)	<X11/Xaw3d/header>
#else
# define XAW_V(header)	<X11/Xaw/header>
#endif

#include XAW_V(Command.h)
#include XAW_V(Label.h)
#include XAW_V(Dialog.h)
#include XAW_V(Box.h)
#include XAW_V(Form.h)
#include XAW_V(Cardinals.h)
#include XAW_V(Text.h)
#include XAW_V(AsciiText.h)
#include XAW_V(MenuButton.h)
#include XAW_V(Scrollbar.h)
#include XAW_V(Simple.h)
#include XAW_V(SimpleMenu.h)
#include XAW_V(Sme.h)
#include XAW_V(SmeLine.h)
#include XAW_V(Tip.h)
#include XAW_V(Toggle.h)
#include XAW_V(Paned.h)
#include XAW_V(Viewport.h)
#include XAW_V(List.h)
#include XAW_V(SmeBSB.h)

#ifdef XAW3D
# define XAWTIPENABLE(widget, text)	XawTipEnable(widget, text)
#else
# define XAWTIPENABLE(widget, text)	do {	\
	XawTipEnable(widget);			\
	FirstArg(XtNtip, text);			\
	SetValues(widget);		} while (0)
#endif
#endif /* FIGX_H */
