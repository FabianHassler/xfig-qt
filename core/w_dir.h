/*
 * FIG : Facility for Interactive Generation of figures
 * Copyright (c) 1985-1988 by Supoj Sutanthavibul
 * Parts Copyright (c) 1989-2015 by Brian V. Smith
 * Parts Copyright (c) 1990 by Digital Equipment Corporation. All Rights Reserved.
 * Parts Copyright (c) 1990 by Digital Equipment Corporation
 * Parts Copyright (c) 1991 by Paul King
 * Parts Copyright (c) 2016-2022 by Thomas Loimer
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
 * Part of the code in this file is taken from xdir,
 * an X-based directory browser.
 */

/*
 *
 * Original xdir code:
 *
 * This file is part of xdir, an X-based directory browser.
 *
 *	Created: 13 Aug 88
 *
 *	Win Treese
 *	Cambridge Research Lab
 *	Digital Equipment Corporation
 *	treese@crl.dec.com
 *
 *	    COPYRIGHT 1990
 *	  DIGITAL EQUIPMENT CORPORATION
 *	   MAYNARD, MASSACHUSETTS
 *	  ALL RIGHTS RESERVED.
 *
 * THE INFORMATION IN THIS SOFTWARE IS SUBJECT TO CHANGE WITHOUT NOTICE AND
 * SHOULD NOT BE CONSTRUED AS A COMMITMENT BY DIGITAL EQUIPMENT CORPORATION.
 * DIGITAL MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THIS SOFTWARE
 * FOR ANY PURPOSE.  IT IS SUPPLIED "AS IS" WITHOUT EXPRESS OR IMPLIED
 * WARRANTY.
 *
 * IF THE SOFTWARE IS MODIFIED IN A MANNER CREATING DERIVATIVE COPYRIGHT
 * RIGHTS, APPROPRIATE LEGENDS MAY BE PLACED ON THE DERIVATIVE WORK IN
 * ADDITION TO THAT SET FORTH ABOVE.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Digital Equipment Corporation not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 *
 */

#ifndef W_DIR_H
#define W_DIR_H

#if defined HAVE_CONFIG_H && !defined VERSION
#include "config.h"		/* restrict */
#endif

#include <X11/Intrinsic.h>	/* includes X11/Xlib.h */


/* Useful constants. */

#define NENTRIES	100	/* chunk size for allocating filename space */
#define F_FILE_WIDTH	413	/* width of filename, etc widgets for FILE panel */
#define E_FILE_WIDTH	360	/* width of filename, etc widgets for EXPORT panel */


extern Widget	popup_dir_text;
extern void	create_dirinfo(Boolean file_exp, Widget parent, Widget below, Widget *ret_beside, Widget *ret_below, Widget *mask_w, Widget *dir_w, Widget *flist_w, Widget *dlist_w, int file_width, Boolean file_panel);
extern void	update_file_export_dir(const char *restrict dir);

/* Xdir function declarations. */

extern char	       *SaveString();
extern void		MakeFullPath(char *root, char *filename, char *pathname);
extern Boolean		IsDirectory(char *path, char *file);
extern void Rescan (Widget widget, XEvent *event, String *params, Cardinal *num_params);

#endif /* W_DIR_H */
