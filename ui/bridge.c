/*
 * bridge.c - calls from the Qt user interface into xfig's C code.
 */
#include "bridge.h"

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
