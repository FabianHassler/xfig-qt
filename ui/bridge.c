/*
 * bridge.c - calls from the Qt user interface into xfig's C code.
 */
#include "bridge.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "resources.h"
#include "object.h"
#include "mode.h"
#include "f_load.h"
#include "u_redraw.h"
#include "w_canvas.h"
#include "w_drawprim.h"
#include "w_file.h"
#include "w_rulers.h"
#include "w_setup.h"
#include "w_util.h"
#include "w_zoom.h"
#include "w_msgpanel.h"
#include "w_modepanel.h"
#include "w_indpanel.h"
#include "f_util.h"
#include "f_save.h"
#include "w_cmdpanel.h"
#include "e_compound.h"
#include "u_print.h"
#include "w_export.h"
#include "xq_widget.h"
#include "paintop.h"

extern Widget	tool, canvas_sw, topruler_sw, sideruler_sw, msg_panel, mousefun,
		unitbox_sw, layer_canvas;

/* the sizes of the ruler views; the rulers must cover them */
static int	topruler_view_wd, sideruler_view_ht;

static void
fit_rulers(void)
{
	if (TOPRULER_WD < topruler_view_wd)
		TOPRULER_WD = topruler_view_wd;
	if (SIDERULER_HT < sideruler_view_ht)
		SIDERULER_HT = sideruler_view_ht;
	if (topruler_win)
		resize_topruler();
	if (sideruler_win)
		resize_sideruler();
}

void
xqb_canvas_resized(int width, int height)
{
	setup_sizes(width, height);
	/* early resizes come before xfig made its graphics contexts */
	if (gccache[PAINT] == NULL)
		return;
	reset_clip_window();
	fit_rulers();
	/* xfig ignores the Expose that follows a resize (ignore_exp_cnt) */
	redisplay_canvas();
}

/* xfig resizes its own windows at times (resize_all() when a file changes
   the orientation) and sets CANVAS_WD/HT to the size it asked for.  Qt
   lays out the main window, so the canvas keeps its size: put xfig's idea
   of it back, or it redraws only part of the canvas. */
void
xqb_canvas_sync(int width, int height)
{
	if (width != CANVAS_WD || height != CANVAS_HT)
		xqb_canvas_resized(width, height);
}

void
xqb_rulers_resized(int topruler_wd, int sideruler_ht)
{
	if (topruler_wd > 0)
		topruler_view_wd = topruler_wd;
	if (sideruler_ht > 0)
		sideruler_view_ht = sideruler_ht;
	if (gccache[PAINT] != NULL)
		fit_rulers();
}

void
xqb_redraw(void)
{
	redisplay_canvas();
}

int
xqb_load_file(const char *path)
{
	return load_file((char *)path, 0, 0);
}

int
xqb_save_file(const char *path)
{
	return write_file((char *)path, True);
}

/* ---------------------------------------- the native file dialogs */

/* What xfig's file panel does once a file is chosen (do_load, do_merge,
   do_save in w_file.c), for a path from the macOS file dialog.  The file
   panel's preview, which these skip, is what its color saving is for. */

/* split an absolute path; dir and name are PATH_MAX buffers */
static int
split_path(const char *path, char *dir, char *name)
{
	const char *slash = strrchr(path, '/');
	if (!slash || strlen(path) >= PATH_MAX)
		return -1;
	if (slash == path) {
		strcpy(dir, "/");
	} else {
		memcpy(dir, path, slash - path);
		dir[slash - path] = '\0';
	}
	strcpy(name, slash + 1);
	return 0;
}

/* -right: mode panel on the right, side ruler left of the canvas */
int
xqb_rhs_panel(void)
{
	return appres.RHS_PANEL;
}

/* in the middle of drawing or editing: xfig says so in the message panel */
int
xqb_busy(void)
{
	return check_action_on();
}

int
xqb_needs_saving(void)
{
	return !emptyfigure() && figure_modified;
}

int
xqb_open(const char *path)
{
	char	dir[PATH_MAX], name[PATH_MAX];

	if (check_action_on() || split_path(path, dir, name))
		return -1;
	restore_user_colors();
	restore_nuser_colors();
	if (change_directory(dir))
		return -1;
	strcpy(cur_file_dir, dir);
	strcpy(cur_export_dir, dir);
	return load_file(name, 0, 0);
}

int
xqb_merge(const char *path)
{
	if (check_action_on())
		return -1;
	merge_file((char *)path, 0, 0);
	return 0;
}

int
xqb_save_as(const char *path)
{
	char	dir[PATH_MAX], name[PATH_MAX];

	if (check_action_on() || emptyfigure_msg("Save") ||
			split_path(path, dir, name))
		return -1;
	restore_user_colors();
	restore_nuser_colors();
	/* inside an opened compound: save all of the figure or this part */
	if ((F_compound *)objects.parent != NULL) {
		int r = popup_query(QUERY_ALLPARTCAN, "You have opened a "
			"compound. You may save just\nthe visible part or all "
			"of the figure.");
		if (r == RESULT_CANCEL)
			return -1;
		if (r == RESULT_ALL)
			close_all_compounds();
	}
	if (change_directory(dir))
		return -1;
	/* image paths in the file are written relative to cur_file_dir */
	strcpy(cur_file_dir, dir);
	/* the dialog asked before replacing a file */
	warnexist = False;
	if (!ok_to_write(name, "SAVE"))
		return -1;
	if (appres.write_bak == True)
		(void)renamefile(name);
	if (write_file(name, True) != 0)
		return -1;
	update_cur_filename(name);
	reset_modifiedflag();
	return 0;
}

/* ------------------------------------------------ the native export dialog */

/* xfig's export languages: lang_items[] (fig2dev's names), lang_texts[] */
int
xqb_num_export_langs(void)
{
	return NUM_EXP_LANG;
}

const char *
xqb_export_lang_text(int lang)
{
	return lang_texts[lang];
}

/* the file name suffix, as in update_def_filename() */
const char *
xqb_export_suffix(int lang)
{
	switch (lang) {
	case LANG_TIFF:		return "tif";
	case LANG_JPEG:		return "jpg";
	case LANG_PDFTEX:	return "pdf";
	case LANG_PSPDF:	return "eps";
	default:		return lang_items[lang];
	}
}

int
xqb_export_lang(void)
{
	return cur_exp_lang;
}

static Widget
find_named(Widget w, const char *name)
{
	if (!w)
		return NULL;
	if (!strcmp(xq_widget_name(w), name))
		return w;
	for (int i = 0; i < xq_widget_num_children(w); ++i) {
		Widget f = find_named(xq_widget_child(w, i), name);
		if (f)
			return f;
	}
	return NULL;
}

/*
 * What the export panel's Export button does, for the language and the
 * path chosen in the file dialog: xfig's export panel keeps its other
 * settings (magnification, margin, background, grid...).
 */
int
xqb_export(const char *path, int lang)
{
	char	dir[PATH_MAX], name[PATH_MAX];

	if (lang < 0 || lang >= NUM_EXP_LANG || check_action_on() ||
			split_path(path, dir, name))
		return -1;
	/* the panel holds the export settings: make it, if not yet there
	   (making it also sets the default export name, set below) */
	if (!export_popup) {
		popup_export_panel(tool);
		XtPopdown(export_popup);
	}
	/* choose the language in the panel's menu, which also updates the
	   panel (lang_select() in w_export.c) */
	Widget entry = find_named(export_popup, lang_texts[lang]);
	if (entry)
		xq_call_callbacks(entry, XtNcallback, NULL);
	else
		cur_exp_lang = lang;
	/* do_export() takes a name typed in the panel first */
	if (exp_selfile)
		XtVaSetValues(exp_selfile, XtNstring, "", NULL);
	if (change_directory(dir))
		return -1;
	strcpy(cur_export_dir, dir);
	strcpy(default_export_file, name);
	do_export(tool);
	return 0;
}

/* ------------------------------------------------- the native print dialog */

/* xfig's print output, PostScript from fig2dev with the print settings
   (magnification, paper, orientation, layers...), written to file */
int
xqb_print_to_file(const char *file)
{
	int	fd, stat;

	if (check_action_on() || emptyfigure_msg("Print"))
		return -1;
	if ((fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0600)) < 0) {
		file_msg("Cannot write print file %s: %s", file, strerror(errno));
		return -1;
	}
	stat = print_to_batchfile(fd, "", "");
	close(fd);
	return stat ? -1 : 0;
}

void
xqb_message(const char *msg)
{
	put_msg("%s", msg);
}

/* the current figure's absolute path, "" if it has no name yet */
const char *
xqb_current_path(void)
{
	static char	path[2 * PATH_MAX];

	if (emptyname(cur_filename))
		path[0] = '\0';
	else if (cur_filename[0] == '/')
		strcpy(path, cur_filename);
	else
		snprintf(path, sizeof path, "%s/%s", cur_file_dir, cur_filename);
	return path;
}

const char *
xqb_current_file(void)
{
	return cur_filename;
}

const char *
xqb_current_dir(void)
{
	return cur_file_dir;
}

int
xqb_figure_modified(void)
{
	return figure_modified;
}

double
xqb_zoom(void)
{
	return display_zoomscale;
}

void
xqb_set_zoom(double z)
{
	if (z < MIN_ZOOM)
		z = MIN_ZOOM;
	if (z > MAX_ZOOM)
		z = MAX_ZOOM;
	display_zoomscale = z;
	zoomscale = display_zoomscale / ZOOM_FACTOR;
	show_zoom(&ind_switches[ZOOM_SWITCH_INDEX]);
	reset_rulers();
	redisplay_canvas();
}

void
xqb_zoom_to_fit(void)
{
	fit_zoom(NULL);
}

void
xqb_unzoom(void)
{
	unzoom();
}

Widget xqb_canvas_widget(void) { return canvas_sw; }
Widget xqb_msg_widget(void) { return msg_panel; }

int xqb_num_modes(void) { return NUM_MODE_SW; }

int xqb_num_menus(void) { return num_main_menus(); }
const char *xqb_menu_title(int i) { return main_menus[i].label; }
Widget xqb_menu_widget(int i) { return main_menus[i].menuwidget; }

void
xqb_quit(void)
{
	quit(NULL, NULL, NULL);
}

extern mode_sw_info mode_switches[];

/* the mode panel button of mode m (F_BOX, ...) */
Widget
xqb_mode_button(int m)
{
	for (int i = 0; i < NUM_MODE_SW; ++i)
		if (mode_switches[i].mode == m)
			return mode_switches[i].widget;
	return NULL;
}

Widget xqb_tool_widget(void) { return tool; }
