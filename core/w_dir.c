/*
 * FIG : Facility for Interactive Generation of figures
 * Copyright (c) 1985-1988 by Supoj Sutanthavibul
 * Parts Copyright (c) 1989-2015 by Brian V. Smith
 * Parts Copyright (c) 1990 by Digital Equipment Corporation. All Rights Reserved.
 * Parts Copyright (c) 1990 by Digital Equipment Corporation
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
 *
 * Part of the code in this file is taken from xdir,
 * an X-based directory browser.
 *
 *
 * Original xdir code:
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

#ifdef HAVE_CONFIG_H
#include "config.h"		/* restrict */
#endif
#include "w_dir.h"

#include <X11/StringDefs.h>
#include <X11/Xlib.h>

#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "dirstruct.h"

#include "figx.h"
#include "resources.h"
#include "mode.h"
#include "f_util.h"
#include "u_error.h"
#include "w_browse.h"
#include "w_cursor.h"
#include "w_drawprim.h"		/* for max_char_height */
#include "w_export.h"
#include "w_file.h"
#include "w_listwidget.h"
#include "w_msgpanel.h"
#include "w_setup.h"
#include "w_util.h"


static char	CurrentSelectionName[PATH_MAX];
static char    *dirmask;

static Widget	hidden;
static Boolean	show_hidden = False;
static Boolean	actions_added=False;

/* Functions */
static void	DoChangeDir(char *dir);
static void	SetDir(Widget widget, XEvent *event, String *params,
				Cardinal *num_params);
static void	ParentDir(Widget w, XEvent *event, String *params,
				Cardinal *num_params);
static Boolean	MakeFileList(char *dir_name, char *mask, char ***dirlist,
				char ***filelist);
static void	parseuserpath(const char *restrict path,
				char *restrict longpath);

static int	wild_match (char *string, char *pattern);
static void	NewList (Widget listwidget, String *list);

/* Static variables */

DeclareStaticArgs(15);
static String	dir_translations =
		    "<Key>Return: SetDir()\n\
		    Ctrl<Key>X: EmptyTextKey()\n\
		    <Key>F18: PastePanelKey()";

static String	list_panel_translations =
		    "<Btn3Up>: ParentDir()";

static String	mask_text_translations =
		    "<Key>Return: Rescan()\n\
		    Ctrl<Key>J: Rescan()\n\
		    Ctrl<Key>M: Rescan()";

static XtActionsRec actionTable[] = {
    {"ParentDir", (XtActionProc)ParentDir},
    {"SetDir", (XtActionProc)SetDir},
    {"Rescan", (XtActionProc)Rescan},
};

/* Function:	FileSelected() is called when the user selects a file.
 *		Set the global variable "CurrentSelectionName"
 *		and set either the export or file panel file name, whichever is popped up
 *		Also, for the file popup, show a preview of the figure.
 * Arguments:	Standard Xt callback arguments.
 * Returns:	Nothing.
 * Notes:
 */



void
FileSelected(Widget w, XtPointer client_data, XtPointer call_data)
{
	(void)w;
	(void)client_data;

    XawListReturnStruct *ret_struct = (XawListReturnStruct *) call_data;

    strcpy(CurrentSelectionName, ret_struct->string);
    FirstArg(XtNstring, CurrentSelectionName);
    if (browse_up) {
	SetValues(browse_selfile);
	XawTextSetInsertionPoint(browse_selfile, strlen(CurrentSelectionName));
    } else if (file_up) {
	SetValues(file_selfile);
	XawTextSetInsertionPoint(file_selfile, strlen(CurrentSelectionName));
	/* and show a preview of the figure in the preview canvas */
	preview_figure(CurrentSelectionName, file_popup, preview_widget,
			preview_size);
    } else if (export_up) {
	SetValues(exp_selfile);
	XawTextSetInsertionPoint(exp_selfile, strlen(CurrentSelectionName));
    }
    /* if nothing is selected it probably means that the user was impatient and
	double clicked the file name again while it was still loading the first */
}

/* Function:	DirSelected() is called when the user selects a directory.
 *
 * Arguments:	Standard Xt callback arguments.
 * Returns:	Nothing.
 * Notes:
 */

void
DirSelected(Widget w, XtPointer client_data, XtPointer call_data)
{
	(void)w;
	(void)client_data;

    XawListReturnStruct *ret_struct = (XawListReturnStruct *) call_data;

    strcpy(CurrentSelectionName, ret_struct->string);
    DoChangeDir(CurrentSelectionName);
}

void
ShowHidden(Widget w, XtPointer client_data, XtPointer ret_val)
{
	(void)w; (void)client_data; (void)ret_val;

    show_hidden = !show_hidden;
    FirstArg(XtNlabel, show_hidden? "Hide Hidden": "Show Hidden");
    SetValues(hidden);
    /* make it rescan the current directory */
    DoChangeDir(".");
}

/* if the export dir diverged from the file dir, leave it there */
void
update_file_export_dir(const char *restrict dir)
{
    if (!strcmp(cur_file_dir, cur_export_dir))
	strcpy(cur_export_dir, dir);
    strcpy(cur_file_dir, dir);
}


void
GoHome(Widget w, XtPointer client_data, XtPointer ret_val)
{
	(void)w; (void)client_data; (void)ret_val;
    char	    dir[PATH_MAX];

    parseuserpath("~",dir);
    if (browse_up)
	strcpy(cur_browse_dir,dir);
    else if (file_up)
	update_file_export_dir(dir);
    else if (export_up)
	strcpy(cur_export_dir,dir);
    DoChangeDir(dir);
}

/*
   come here when the user presses return in the directory path widget
   Get the current string from the widget and set the current directory to that
   Also, copy the dir to the current directory widget in the file popup
*/

/* Function:  SetDir() changes to the parent directory.
 * Arguments: Standard Xt action arguments.
 * Returns:   Nothing.
 * Notes:
 */

void
SetDir(Widget widget, XEvent *event, String *params, Cardinal *num_params)
{
	(void)widget; (void)event; (void)params; (void)num_params;
    char	longdir[PATH_MAX];
    char	   *ndir;

    /* get the string from the widget */
    FirstArg(XtNstring, &ndir);
    if (browse_up) {
	GetValues(browse_dir);
	strcpy(cur_browse_dir,ndir);	/* save in global var */
    } else if (file_up) {
	GetValues(file_dir);
	update_file_export_dir(ndir);
    } else if (export_up) {
	GetValues(exp_dir);
	strcpy(cur_export_dir,ndir);	/* save in global var */
    }
    /* if there is a ~ in the directory, parse the username */
    if (ndir[0]=='~') {
	parseuserpath(ndir,longdir);
	ndir=longdir;
    }
    DoChangeDir(ndir);
}

/*
 * make the full path from ~/partialpath
 * Parse ~somewhere as ~/somewhere, do not go to other user's home directories.
 */
static void
parseuserpath(const char *restrict path, char *restrict longpath)
{
	/* parseuserpath is called with  path[0] == '~' */
	strcpy(longpath, getenv("HOME"));
	if (strlen(path) == 1)
		return;		/* nothing after the ~, we have the full path */
	if (path[1] != '/')
		strcat(longpath, "/");
	strcat(longpath, &path[1]);	/* append the rest of the path */
	return;
}


/* the file_exp parm just changes the vertical offset for the Rescan button */
/* make the filename list file_width wide */

void
create_dirinfo(Boolean file_exp, Widget parent, Widget below, Widget *ret_beside, Widget *ret_below, Widget *mask_w, Widget *dir_w, Widget *flist_w, Widget *dlist_w, int file_width, Boolean file_panel)
{
	(void)file_exp;
    Widget	    w,dir_alt,home;
    Widget	    file_viewport;
    Widget	    dir_viewport;
    XFontStruct	   *temp_font;
    int		    char_ht,char_wd;
    char	   *dir;
    char	  **dirlist, **filelist;

    /* Initialize _filelist and _dirlist */
    MakeFileList(NULL, NULL, NULL, NULL);

    if (browse_up) {
	get_directory(cur_browse_dir);
	dir = cur_browse_dir;
    } else if (export_up) {
	get_directory(cur_export_dir);
	dir = cur_export_dir;
    } else {
	get_directory(cur_file_dir);
	dir = cur_file_dir;
    }

    if (file_up) {
	FirstArg(XtNlabel, "Fig files");
    } else {
	FirstArg(XtNlabel, "     Existing");
    }
    NextArg(XtNinternational, False);
    NextArg(XtNfromVert, below);
    NextArg(XtNborderWidth, 0);
    NextArg(XtNtop, XtChainTop);
    NextArg(XtNbottom, XtChainTop);
    NextArg(XtNleft, XtChainLeft);
    NextArg(XtNright, XtChainLeft);
    w = XtCreateManagedWidget("file_alt_label", labelWidgetClass,
			      parent, Args, ArgCount);

    FirstArg(XtNfont, &temp_font);
    GetValues(w);
    char_ht = max_char_height(temp_font) + 2;
    char_wd = char_width(temp_font) + 2;

    /* make a viewport to hold the list widget of filenames */
    FirstArg(XtNallowVert, True);
    if (file_up) {
	/* for the file panel, put the viewport below the Alternatives label */
	NextArg(XtNfromVert, w);
	NextArg(XtNheight, char_ht * 15);	/* show first 15 filenames */
    } else {
	/* for the export or browse panel, put the viewport beside the Alternatives label */
	NextArg(XtNfromVert, below);
	NextArg(XtNfromHoriz, w);
	if (browse_up) {
	    NextArg(XtNheight, char_ht * 10);	/* show 10 lines for existing browse files */
	} else {
	    NextArg(XtNheight, char_ht * 4);	/* show 4 lines for existing export files */
	}
    }
    NextArg(XtNborderWidth, INTERNAL_BW);
    NextArg(XtNwidth, file_width);
    NextArg(XtNtop, XtChainTop);		/* chain so the viewport resizes fully */
    NextArg(XtNbottom, XtChainBottom);
    NextArg(XtNleft, XtChainLeft);
    NextArg(XtNright, XtChainRight);
    file_viewport = XtCreateManagedWidget("vport", viewportWidgetClass,
					  parent, Args, ArgCount);

    /* label for filename mask */

    FirstArg(XtNlabel, "Filename Mask");
    NextArg(XtNinternational, False);
    NextArg(XtNborderWidth, 0);
    NextArg(XtNfromVert, file_viewport);
    NextArg(XtNtop, XtChainBottom);
    NextArg(XtNbottom, XtChainBottom);
    NextArg(XtNleft, XtChainLeft);
    NextArg(XtNright, XtChainLeft);
    w = XtCreateManagedWidget("mask_label", labelWidgetClass,
				parent, Args, ArgCount);

    /* text widget for the filename mask */

    FirstArg(XtNeditType, XawtextEdit);
    NextArg(XtNinternational, appres.international);
    NextArg(XtNleftMargin, 4);
    NextArg(XtNheight, char_ht * 2);
    NextArg(XtNscrollHorizontal, XawtextScrollWhenNeeded);
    NextArg(XtNborderWidth, INTERNAL_BW);
    NextArg(XtNwidth, file_panel? F_FILE_WIDTH: E_FILE_WIDTH);
    NextArg(XtNfromHoriz, w);
    NextArg(XtNfromVert, file_viewport);
    NextArg(XtNtop, XtChainBottom);
    NextArg(XtNbottom, XtChainBottom);
    NextArg(XtNleft, XtChainLeft);
    NextArg(XtNright, XtChainRight);
    *mask_w = XtCreateManagedWidget("mask", asciiTextWidgetClass,
					parent, Args, ArgCount);
    XtOverrideTranslations(*mask_w, XtParseTranslationTable(mask_text_translations));

    /* get the first directory listing */

    FirstArg(XtNstring, &dirmask);
    GetValues(*mask_w);
    if (MakeFileList(dir, dirmask, &dirlist, &filelist) == False)
	file_msg("No files in directory?");

    FirstArg(XtNlabel, "  Current Dir");
    NextArg(XtNinternational, False);
    NextArg(XtNborderWidth, 0);
    NextArg(XtNfromVert, *mask_w);
    NextArg(XtNtop, XtChainBottom);
    NextArg(XtNbottom, XtChainBottom);
    NextArg(XtNleft, XtChainLeft);
    NextArg(XtNright, XtChainLeft);
    w = XtCreateManagedWidget("dir_label", labelWidgetClass,
			      parent, Args, ArgCount);

    FirstArg(XtNstring, dir);
    NextArg(XtNinternational, appres.international);
    NextArg(XtNleftMargin, 4);
    NextArg(XtNinsertPosition, strlen(dir));
    NextArg(XtNheight, char_ht * 2);
    NextArg(XtNborderWidth, INTERNAL_BW);
    NextArg(XtNscrollHorizontal, XawtextScrollWhenNeeded);
    NextArg(XtNeditType, XawtextEdit);
    NextArg(XtNfromVert, *mask_w);
    NextArg(XtNfromHoriz, w);
    NextArg(XtNwidth, file_panel? F_FILE_WIDTH: E_FILE_WIDTH);
    NextArg(XtNtop, XtChainBottom);
    NextArg(XtNbottom, XtChainBottom);
    NextArg(XtNleft, XtChainLeft);
    NextArg(XtNright, XtChainRight);
    *dir_w = XtCreateManagedWidget("dir_name", asciiTextWidgetClass,
				   parent, Args, ArgCount);

    XtOverrideTranslations(*dir_w, XtParseTranslationTable(dir_translations));

    /* directory alternatives */

    FirstArg(XtNlabel, "  Directories");
    NextArg(XtNinternational, False);
    NextArg(XtNborderWidth, 0);
    NextArg(XtNfromVert, *dir_w);
    NextArg(XtNtop, XtChainBottom);
    NextArg(XtNbottom, XtChainBottom);
    NextArg(XtNleft, XtChainLeft);
    NextArg(XtNright, XtChainLeft);
    dir_alt = XtCreateManagedWidget("dir_alt_label", labelWidgetClass,
			      parent, Args, ArgCount);

    /* put a Home button to the left of the list of directories */
    FirstArg(XtNlabel, "Home");
    NextArg(XtNinternational, False);
    NextArg(XtNfromVert, dir_alt);
    NextArg(XtNfromHoriz, dir_alt);
    NextArg(XtNhorizDistance, -(char_wd * 5));
    NextArg(XtNborderWidth, INTERNAL_BW);
    NextArg(XtNtop, XtChainBottom);
    NextArg(XtNbottom, XtChainBottom);
    NextArg(XtNleft, XtChainLeft);
    NextArg(XtNright, XtChainLeft);
    home = XtCreateManagedWidget("home", commandWidgetClass, parent, Args, ArgCount);
    XtAddCallback(home, XtNcallback, GoHome, (XtPointer) NULL);

    /* put a button for showing/hiding hidden files below the Home button */

    FirstArg(XtNlabel, show_hidden? "Hide Hidden": "Show Hidden");
    NextArg(XtNinternational, False);
    NextArg(XtNfromVert, home);
    NextArg(XtNfromHoriz, dir_alt);
    NextArg(XtNhorizDistance, (int) -(char_wd * 10.5));
    NextArg(XtNborderWidth, INTERNAL_BW);
    NextArg(XtNtop, XtChainBottom);
    NextArg(XtNbottom, XtChainBottom);
    NextArg(XtNleft, XtChainLeft);
    NextArg(XtNright, XtChainLeft);
    hidden = XtCreateManagedWidget("hidden", commandWidgetClass,
				parent, Args, ArgCount);
    XtAddCallback(hidden, XtNcallback, ShowHidden, (XtPointer) NULL);

    FirstArg(XtNallowVert, True);
    NextArg(XtNforceBars, True);
    NextArg(XtNfromHoriz, dir_alt);
    NextArg(XtNfromVert, *dir_w);
    NextArg(XtNborderWidth, INTERNAL_BW);
    NextArg(XtNwidth, file_panel? F_FILE_WIDTH: E_FILE_WIDTH);
    NextArg(XtNheight, char_ht * 5);
    NextArg(XtNtop, XtChainBottom);
    NextArg(XtNbottom, XtChainBottom);
    NextArg(XtNleft, XtChainLeft);
    NextArg(XtNright, XtChainRight);
    dir_viewport = XtCreateManagedWidget("dirvport", viewportWidgetClass,
					 parent, Args, ArgCount);

    FirstArg(XtNlist, filelist);
    NextArg(XtNinternational, appres.international);
    /* for file panel use only one column */
    if (file_panel) {
	NextArg(XtNdefaultColumns, 1);
	NextArg(XtNforceColumns, True);
    }
    *flist_w = XtCreateManagedWidget("file_list_panel", figListWidgetClass,
				     file_viewport, Args, ArgCount);
    XtAddCallback(*flist_w, XtNcallback, FileSelected, (XtPointer) NULL);
    XtOverrideTranslations(*flist_w,
			   XtParseTranslationTable(list_panel_translations));

    FirstArg(XtNlist, dirlist);
    NextArg(XtNinternational, appres.international);
    *dlist_w = XtCreateManagedWidget("dir_list_panel", figListWidgetClass,
				     dir_viewport, Args, ArgCount);
    XtOverrideTranslations(*dlist_w,
			   XtParseTranslationTable(list_panel_translations));

    XtAddCallback(*dlist_w, XtNcallback, DirSelected, (XtPointer) NULL);

    if (!actions_added) {
	XtAppAddActions(tool_app, actionTable, XtNumber(actionTable));
	actions_added = True;
    }

    FirstArg(XtNlabel, "Rescan");
    NextArg(XtNinternational, False);
    NextArg(XtNfromVert, dir_viewport);
    NextArg(XtNvertDistance, 15);
    NextArg(XtNborderWidth, INTERNAL_BW);
    NextArg(XtNhorizDistance, 45);
    NextArg(XtNheight, 25);
    NextArg(XtNtop, XtChainBottom);
    NextArg(XtNbottom, XtChainBottom);
    NextArg(XtNleft, XtChainLeft);
    NextArg(XtNright, XtChainLeft);
    w = XtCreateManagedWidget("rescan", commandWidgetClass, parent,
			      Args, ArgCount);
    XtAddCallback(w, XtNcallback, (XtCallbackProc)Rescan, NULL);

    /* install accelerators so they can be used from each window */
    XtInstallAccelerators(parent, w);
    XtInstallAccelerators(*flist_w, parent);
    XtInstallAccelerators(*dlist_w, parent);

    *ret_beside = w;
    *ret_below = dir_viewport;

    return;
}

#ifdef FREEMEM

static void
free_list(char **restrict list, const int count)
{
	char	**entry;
	for (entry = list; entry - list < count; ++entry)
		if (*entry)
			free(*entry);
	free(list);
}

static void
freelists(void)
{
	char	**dummy;
	(void)MakeFileList(NULL, NULL, &dummy, NULL);
}

#endif /* FREEMEM */

/* Function:	SPComp() compares two string pointers for qsort().
 * Arguments:	s1, s2: strings to be compared.
 * Returns:	Value of strcmp().
 * Notes:	man qsort(3) shows the cast in the EXAMPLES section.
 */

static int
SPComp(const void *p1, const void *p2)
{
    return (strcmp(*(const char **)p1, *(const char **)p2));
}

static void
replace_entry(char **restrict entry, const char *new)
{
	if (!strcmp(*entry, new))
		return;
	if (*entry)
		free(*entry);
	*entry = strdup(new);
}

/*
 * Double the size of the file/directory list.
 * Return the newly allocated list and the new size of the list.
 */
static void
extend_list(char ***restrict list, char ***restrict current, int *num)
{
	char		**c;
	const int	fac = 2;

	*list = realloc(*list, *num * fac * sizeof(char *));

	/* zero the string pointers to know when free() is possible */
	for (c = *list + *num; c < *list + fac * *num; ++c)
		*c = NULL;
	*current = *list + *num - 1;
	*num *= fac;
}


#define MAX_MASKS	20
#define MAX_MASK_LEN	64
#define _STRINGIFY(X)	#X
#define STRINGIFY(X)	_STRINGIFY(X)

/*
 * Use MakeFileList(NULL, NULL, NULL, NULL) to allocate memory for the
 * statically allocated string arrays _dirlist and _filelist.
 * MakeFileList(NULL, NULL, x, NULL) will free _dirlist and _filelist.
 */
static Boolean
MakeFileList(char *dir_name, char *mask, char ***dirlist, char ***filelist)
{
    static char	**_filelist = NULL;
    static char	**_dirlist = NULL;
    static int	file_entry_cnt = NENTRIES;
    static int	dir_entry_cnt = NENTRIES;
    DIR		  *dirp;
    DIRSTRUCT	  *dp;
    char	 **cur_file, **cur_directory;
    int		   nmasks,i;
    char	  *wild[MAX_MASKS],*cmask;
    char	buf[MAX_MASK_LEN];
    const char	delims[] = " \t";
    Boolean	   match;

    if (dir_name == NULL) {
#ifdef FREEMEM
	/* at exit, call with MakeFileList(NULL, NULL, x, NULL) */
	if (dirlist != NULL) {
		if (_filelist)
			free_list(_filelist, file_entry_cnt);
		if(_dirlist)
			free_list(_dirlist, dir_entry_cnt);
		return True;
	}
#endif
	if (_filelist)	/* already initialized */
	    return True;

	_filelist = calloc(file_entry_cnt, sizeof(char *));
	_dirlist = calloc(dir_entry_cnt, sizeof(char *));
	if (!_filelist || !_dirlist) {
		put_msg("No more memory - QUITTING!");
		fputs("Available memory exceeded - quitting.\n", stderr);
		emergency_quit(False);
	}
#ifdef FREEMEM
	atexit(freelists);
#endif
	return True;
    }

    set_temp_cursor(wait_cursor);
    cur_file = _filelist;
    cur_directory = _dirlist;

    dirp = opendir(dir_name);
    if (dirp == NULL) {
	reset_cursor();
	replace_entry(_filelist, "");
	replace_entry(_dirlist, "..");
	*filelist = _filelist;
	return False;
    }
    /* process any events to ensure cursor is set to wait_cursor */
    /*
     * don't do this inside the following loop because this procedure could
     * be re-entered if the user presses (e.g.) rescan
     */
    app_flush();

    /* make copy of mask */
    if (strlen(mask) < MAX_MASK_LEN) {
	    cmask = buf;
	    strcpy(cmask, mask);
    } else {
	    cmask = strdup(mask);
    }
    if (*cmask == '\0')
	strcpy(cmask,"*");
    wild[0] = strtok(cmask, delims);
    nmasks = 1;
    while (nmasks < MAX_MASKS && (wild[nmasks]=strtok((char*)NULL, delims)))
	nmasks++;
    if (nmasks == MAX_MASKS && strtok(NULL, delims))
	file_msg("Maximum number of filename masks exceeded, only first "
				STRINGIFY(MAX_MASKS) " considered.");
    for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
	/* skip over '.' (current dir) */
	if (!strcmp(dp->d_name, "."))
	    continue;
	/* if don't want to see the hidden files (beginning with ".") skip them */
	if (!show_hidden && dp->d_name[0]=='.' && strcmp(dp->d_name,".."))
	    continue;

	if (IsDirectory(dir_name, dp->d_name)) {
	    if (*cur_directory)
		    free(*cur_directory);
	    *cur_directory++ = strdup(dp->d_name);
	    if (cur_directory == _dirlist + dir_entry_cnt - 1)
		extend_list(&_dirlist, &cur_directory, &dir_entry_cnt);
	} else {
	    /* check if matches regular expression */
	    match=False;
	    for (i=0; i<nmasks; i++) {
		if (wild_match(dp->d_name, wild[i])) {
		    int wild_match (char *string, char *pattern);
		    match = True;
		    break;
		}
	    }
	    if (!match)
		continue;	/* no, do next */
	    if (wild[i][0] == '*' && dp->d_name[0] == '.')
		continue;	/* skip files with leading . */
	    if (*cur_file)
		    free(*cur_file);
	    *cur_file++ = strdup(dp->d_name);
	    if (cur_file == _filelist + file_entry_cnt - 1) {
		extend_list(&_filelist, &cur_file, &file_entry_cnt);
	    }
	}
    }
    if (*cur_file) {
	    free(*cur_file);
	    *cur_file = NULL;
    }
    if (*cur_directory) {
	    free(*cur_directory);
	    *cur_directory = NULL;
    }
    if (cmask != buf)
	    free(cmask);
    closedir(dirp);

    *filelist = _filelist;
    *dirlist = _dirlist;

    if (cur_file != _filelist)
	qsort(_filelist, cur_file - _filelist, sizeof(char *), SPComp);
    if (cur_directory != _dirlist)
	qsort(_dirlist, cur_directory - _dirlist, sizeof(char *), SPComp);
    reset_cursor();
    return True;
}

/* Function:	ParentDir() changes to the parent directory.
 * Arguments:	Standard Xt action arguments.
 * Returns:	Nothing.
 * Notes:
 */

static void
ParentDir(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	(void)w; (void)event; (void)params; (void)num_params;
    DoChangeDir("..");
}

/* Function:	DoChangeDir() actually changes the directory and changes
 *		the list widget values to the new listing.
 * Arguments:	dir:	Pathname of new directory.
 * Returns:	Nothing.
 * Notes:
 *	NULL for dir means to rebuild the file list for the current directory
 *	(as in an update to the directory or change in filename filter).
 */

void
DoChangeDir(char *dir)
{
	char	*abs_path = NULL;
	char	**filelist, **dirlist;

	/* resolve the path given in dir to abs_path */
	if (!(abs_path = realpath(dir && *dir ? dir : ".", NULL))) {
		/* realpath knows about the current directory and
		   correctly resolves relative paths. */
		file_msg("Cannot infer absolute path for %s: %s",
				dir && *dir ? dir : "." , strerror(errno));
		return;
	}

	/* change to abs_path */
	if (abs_path && change_directory(abs_path)) {
		free(abs_path);
		/* change_directory already delivered an error message */
		return;
	}

	if (MakeFileList(abs_path, dirmask, &dirlist, &filelist) == False) {
		file_msg("Unable to list directory %s", abs_path);
		free(abs_path);
		return;
	}

	/* update the current directory and file/dir list widgets */
	FirstArg(XtNstring, abs_path);
	if (browse_up) {
		SetValues(browse_dir);
		strcpy(cur_browse_dir, abs_path);	/* update global var */
		XawTextSetInsertionPoint(browse_dir, strlen(abs_path));
		NewList(browse_flist, filelist);
		NewList(browse_dlist, dirlist);
	} else if (file_up) {
		SetValues(file_dir);
		update_file_export_dir(abs_path);
		XawTextSetInsertionPoint(file_dir, strlen(abs_path));
		NewList(file_flist, filelist);
		NewList(file_dlist, dirlist);
	} else if (export_up) {
		SetValues(exp_dir);
		strcpy(cur_export_dir, abs_path);	/* update global var */
		XawTextSetInsertionPoint(exp_dir, strlen(abs_path));
		NewList(exp_flist, filelist);
		NewList(exp_dlist, dirlist);
	}
	free(abs_path);
	CurrentSelectionName[0] = '\0';
}

void
Rescan(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
    char	*dir;
    char	**filelist, **dirlist;

    /*
     * get the mask string from the File or Export mask widget and put in
     * dirmask
     */
    if (browse_up) {
	FirstArg(XtNstring, &dirmask);
	GetValues(browse_mask);
	FirstArg(XtNstring, &dir);
	GetValues(browse_dir);
	if (change_directory(dir))	/* make sure we are there */
	    return;
	strcpy(cur_browse_dir,dir);	/* save in global var */
	(void) MakeFileList(dir, dirmask, &dirlist, &filelist);
	/* List Widgets need to have the string array remain available. */
	NewList(browse_flist, filelist);
	NewList(browse_dlist, dirlist);
    } else if (file_up) {
	FirstArg(XtNstring, &dirmask);
	GetValues(file_mask);
	FirstArg(XtNstring, &dir);
	GetValues(file_dir);
	if (change_directory(dir))	/* make sure we are there */
	    return;
	update_file_export_dir(dir);
	(void) MakeFileList(dir, dirmask, &dirlist, &filelist);
	NewList(file_flist,filelist);
	NewList(file_dlist,dirlist);
    } else if (export_up) {
	FirstArg(XtNstring, &dirmask);
	GetValues(exp_mask);
	FirstArg(XtNstring, &dir);
	GetValues(exp_dir);
	if (change_directory(dir))	/* make sure we are there */
	    return;
	strcpy(cur_export_dir,dir);	/* save in global var */
	(void) MakeFileList(dir, dirmask, &dirlist, &filelist);
	NewList(exp_flist, filelist);
	NewList(exp_dlist, dirlist);
    }
}

static void
NewList(Widget listwidget, String *list)
{
	/* install the new list */
	XawListChange(listwidget, list, 0, 0, True);

	/* install the wheel scrolling for the scrollbar */
	InstallScroll(listwidget);
}

/* Function:	IsDirectory() tests to see if a pathname is a directory.
 * Arguments:	path:	Pathname of file to test.
 *		file:	Filename in path.
 * Returns:	True or False.
 * Notes:	False is returned if the directory is not accessible.
 */

Boolean
IsDirectory(char *path, char *file)
{
    char	    fullpath[PATH_MAX];
    struct stat	    statbuf;

    if (file == NULL)
	return False;
    MakeFullPath(path, file, fullpath);
    if (stat(fullpath, &statbuf))	/* error, report that it is not a directory */
	return False;
    if (statbuf.st_mode & S_IFDIR)
	return True;
    else
	return False;
}

/* Function:	MakeFullPath() creates the full pathname for the given file.
 * Arguments:	filename:	Name of the file in question.
 *		pathname:	Buffer for full name.
 * Returns:	Nothing.
 * Notes:
 */

void
MakeFullPath(char *root, char *filename, char *pathname)
{
    strcpy(pathname, root);
    strcat(pathname, "/");
    strcat(pathname, filename);
}

/* wildmatch.c - Unix-style command line wildcards

   This procedure is in the public domain.

   After that, it is just as if the operating system had expanded the
   arguments, except that they are not sorted.	The program name and all
   arguments that are expanded from wildcards are lowercased.

   Syntax for wildcards:
   *		Matches zero or more of any character (except a '.' at
		the beginning of a name).
   ?		Matches any single character.
   [r3z]	Matches 'r', '3', or 'z'.
   [a-d]	Matches a single character in the range 'a' through 'd'.
   [!a-d]	Matches any single character except a character in the
		range 'a' through 'd'.

   The period between the filename root and its extension need not be
   given explicitly.  Thus, the pattern `a*e' will match 'abacus.exe'
   and 'axyz.e' as well as 'apple'.  Comparisons are not case sensitive.

   The wild_match code was written by Rich Salz, rsalz@bbn.com,
   posted to net.sources in November, 1986.

   The code connecting the two is by Mike Slomin, bellcore!lcuxa!mike2,
   posted to comp.sys.ibm.pc in November, 1988.

   Major performance enhancements and bug fixes, and source cleanup,
   by David MacKenzie, djm@ai.mit.edu. */

/* Shell-style pattern matching for ?, \, [], and * characters.
   I'm putting this replacement in the public domain.

   Written by Rich $alz, mirror!rs, Wed Nov 26 19:03:17 EST 1986. */

/* The character that inverts a character class; '!' or '^'. */
#define INVERT '!'

static int	star(char *string, char *pattern);

/* Return nonzero if `string' matches Unix-style wildcard pattern
   `pattern'; zero if not. */

int
wild_match(char *string, char *pattern)
{
    int		    prev;	/* Previous character in character class. */
    int		    matched;	/* If 1, character class has been matched. */
    int		    reverse;	/* If 1, character class is inverted. */

    for (; *pattern; string++, pattern++)
	switch (*pattern) {
	case '\\':
	    /* Literal match with following character */
	    pattern++;
	    /* intentionally fall through. */
	default:
	    if (*string != *pattern)
		return 0;
	    continue;
	case '?':
	    /* Match anything. */
	    if (*string == '\0')
		return 0;
	    continue;
	case '*':
	    /* Trailing star matches everything. */
	    return *++pattern ? star(string, pattern) : 1;
	case '[':
	    /* Check for inverse character class. */
	    reverse = pattern[1] == INVERT;
	    if (reverse)
		pattern++;
	    for (prev = 256, matched = 0; *++pattern && *pattern != ']';
		 prev = *pattern)
		if (*pattern == '-'
		    ? *string <= *++pattern && *string >= prev
		    : *string == *pattern)
		    matched = 1;
	    if (matched == reverse)
		return 0;
	    continue;
	}

    return *string == '\0';
}

static int
star(char *string, char *pattern)
{
    while (wild_match(string, pattern) == 0)
	if (*++string == '\0')
	    return 0;
    return 1;
}
