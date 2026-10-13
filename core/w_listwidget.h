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

/* FigList widget public header file */

#ifndef W_LISTWIDGET_H
#define W_LISTWIDGET_H

#if defined HAVE_CONFIG_H && !defined VERSION
#include "config.h"
#endif

#include <X11/Intrinsic.h>
#ifdef XAW3D
# include <X11/Xaw3d/Simple.h>
#else
# include <X11/Xaw/Simple.h>
#endif

extern WidgetClass figListWidgetClass;

typedef struct	_FigListClassRec *FigListWidgetClass;
typedef struct	_FigListRec	 *FigListWidget;

#endif
