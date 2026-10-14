/*
 * bridge.c - calls from the Qt user interface into xfig's C code.
 */
#include "bridge.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
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
#include "w_print.h"
#include "xq_widget.h"
#include "paintop.h"
#include "svgout.h"
#include "e_edit.h"
#include "f_picobj.h"

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

/* the last widget named name: make_pulldown_menu() puts separator lines
   of the same name before the first bitmap language's entry */
static Widget
find_last_named(Widget w, const char *name)
{
	Widget	found = NULL;

	if (!w)
		return NULL;
	if (!strcmp(xq_widget_name(w), name))
		found = w;
	for (int i = 0; i < xq_widget_num_children(w); ++i) {
		Widget f = find_last_named(xq_widget_child(w, i), name);
		if (f)
			found = f;
	}
	return found;
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
	Widget entry = find_last_named(export_popup, lang_texts[lang]);
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

/* the print panel's magnification, grid and background, as do_print()
   takes them; grid holds at least 80 characters, backgrnd 10 */
static void
print_settings(char *backgrnd, char *grid)
{
	grid[0] = '\0';
	if (print_popup) {
		appres.magnification = (float)atof(panel_get_value(print_mag_text));
		if (appres.magnification <= 0.0)
			appres.magnification = 100.0;
		print_update_figure_size();
		get_grid_spec(grid, print_grid_minor_text, print_grid_major_text);
	}
	make_rgb_string(export_background_color, backgrnd);
}

/* xfig's print output, PostScript from fig2dev with the print settings
   (magnification, paper, orientation, layers, grid, background...),
   written to file */
int
xqb_print_to_file(const char *file)
{
	int	fd, stat;
	char	backgrnd[10], grid[80];

	if (check_action_on() || emptyfigure_msg("Print"))
		return -1;
	print_settings(backgrnd, grid);
	if ((fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0600)) < 0) {
		file_msg("Cannot write print file %s: %s", file, strerror(errno));
		return -1;
	}
	stat = print_to_batchfile(fd, backgrnd, grid);
	close(fd);
	return stat ? -1 : 0;
}

/*
 * xfig's print panel, as print options: the printer, its command (lp or
 * lpr), job parameters and the batch are the print dialog's business, so
 * these rows go; "Print FIGURE to Printer" opens the print dialog.
 */
static void	(*native_print)(void);

void
xqb_set_native_print(void (*print)(void))
{
	native_print = print;
}

static void
native_print_handler(Widget w, XtPointer client, XEvent *ev, Boolean *cont)
{
	(void)w;
	(void)client;
	(void)ev;
	(void)cont;
	if (native_print)
		native_print();
}

void
xqb_adapt_print_panel(void)
{
	static const char *const gone[] = {"printcommand_label", "printcommand",
		"printer_label", "printer", "printer_names", "job_params_label",
		"job_params", "num_batch_label", "num_batch", "print_batch",
		"clear_batch", NULL};
	static Boolean	done = False;
	Widget		removed[16];
	int		n = 0;
	XtArgVal	anchor = 0;

	if (done || !print_panel)
		return;
	done = True;
	/* the rows hang below the grid row; the print command row is first */
	Widget pc = find_named(print_panel, "printcommand_label");
	if (pc)
		XtVaGetValues(pc, XtNfromVert, &anchor, NULL);
	for (int i = 0; gone[i]; ++i) {
		Widget w = find_named(print_panel, gone[i]);
		if (w) {
			XtUnmanageChild(w);
			removed[n++] = w;
		}
	}
	/* what hung below a removed row hangs below the grid row */
	for (int i = 0; i < xq_widget_num_children(print_panel); ++i) {
		Widget c = xq_widget_child(print_panel, i);
		XtArgVal fv = 0;
		XtVaGetValues(c, XtNfromVert, &fv, NULL);
		for (int k = 0; k < n; ++k)
			if ((Widget)fv == removed[k])
				XtVaSetValues(c, XtNfromVert, anchor, NULL);
	}
	Widget label = find_named(print_panel, "print_label");
	if (label)
		XtVaSetValues(label, XtNlabel, "Print Options", NULL);
	Widget print = find_named(print_panel, "print");
	if (print) {
		XtRemoveEventHandler(print, ButtonReleaseMask, False,
				(XtEventHandler)do_print, NULL);
		XtAddEventHandler(print, ButtonReleaseMask, False,
				native_print_handler, NULL);
		XtVaSetValues(print, XtNlabel, "Print...", NULL);
	}
}

/* Global settings: xfig puts "Track mouse in rulers" over "Autorefresh
   figure" (both have no widget above them); put it below */
void
xqb_adapt_global_panel(Widget shell)
{
	Widget	track = find_named(shell, "track_mouse");
	Widget	autoref = find_named(shell, "auto_refresh");
	XtArgVal fv = 0;

	if (!track || !autoref)
		return;
	XtVaGetValues(track, XtNfromVert, &fv, NULL);
	if (!fv)
		XtVaSetValues(track, XtNfromVert, (XtArgVal)autoref, NULL);
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

/* ----------------------------------------- PDF and bitmaps without Ghostscript */

/*
 * fig2dev makes PDF and bitmaps from its PostScript with Ghostscript, and
 * xfig renders EPS and PDF pictures with it.  Without Ghostscript (the
 * ghostscript resource is empty), fig2dev's SVG is drawn with Qt instead
 * (svgout.cpp), and PDF pictures are rendered with QtPdf.
 */
extern int	gs_bitmap(char *file, F_pic *pic, int llx, int lly,
			int urx, int ury);

static int
have_ghostscript(void)
{
	return appres.ghostscript && appres.ghostscript[0];
}

int
xqb_have_ghostscript(void)
{
	return have_ghostscript();
}

static int
needs_ghostscript(int lang)
{
	return lang == LANG_PDF || lang == LANG_PSPDF || lang == LANG_PDFTEX ||
		lang == LANG_PSPDFTEX || lang >= FIRST_BITMAP_LANG;
}

/* fig2dev's SVG of the figure (with the magnification and the layers)
   into a new temporary file, named in svg[PATH_MAX]; picture paths are
   relative to cur_export_dir */
static int
make_svg(char *svg)
{
	const char	*tmp = getenv("XFIGTMPDIR");
	char		none[1] = "";
	int		fd, ret, lang = cur_exp_lang;
	Boolean		warn = warnexist;
	struct stat	st;

	snprintf(svg, PATH_MAX, "%s/xfig-XXXXXX.svg", tmp && *tmp ? tmp : "/tmp");
	if ((fd = mkstemps(svg, 4)) < 0) {
		file_msg("Cannot create a temporary file %s: %s", svg,
				strerror(errno));
		return -1;
	}
	close(fd);
	cur_exp_lang = LANG_SVG;
	warnexist = False;
	ret = print_export(svg, 0, 0, none, NULL, False, 0, none);
	cur_exp_lang = lang;
	warnexist = warn;
	if (ret || stat(svg, &st) || st.st_size == 0) {
		unlink(svg);
		return -1;
	}
	return 0;
}

/* the paper of the print and export panels */
static void
page_job(struct xq_svg_job *job)
{
	job->page = 1;
	job->paper_wd = paper_sizes[appres.papersize].width * 72.0 / PIX_PER_INCH;
	job->paper_ht = paper_sizes[appres.papersize].height * 72.0 / PIX_PER_INCH;
	job->landscape = appres.landscape;
	job->flushleft = appres.flushleft;
}

/*
 * print_export() for w_export.c: without Ghostscript, PDF and bitmaps
 * come from SVG.
 */
int
xq_print_export(char *file, int xoff, int yoff, char *backgrnd,
		char *transparent, Boolean use_transp_backg, int border,
		char *grid)
{
	char			svg[PATH_MAX];
	struct xq_svg_job	job;
	int			stat;

	if (have_ghostscript() || !needs_ghostscript(cur_exp_lang))
		return print_export(file, xoff, yoff, backgrnd, transparent,
				use_transp_backg, border, grid);

	if (!xq_svg_can_export(lang_items[cur_exp_lang])) {
		file_msg("Export to %s needs Ghostscript (gs), which was not "
				"found.", lang_items[cur_exp_lang]);
		put_msg("Export to \"%s\" failed", file);
		return 1;
	}
	if (!ok_to_write(file, "EXPORT"))
		return 1;
	put_msg("Exporting to file \"%s\" ...", file);
	app_flush();
	if (make_svg(svg)) {
		put_msg("Export to \"%s\" failed", file);
		return 1;
	}
	memset(&job, 0, sizeof job);
	job.svg = svg;
	job.dir = cur_export_dir;
	job.out = file;
	job.format = lang_items[cur_exp_lang];
	job.border = border;
	job.mag = appres.magnification / 100.0;
	job.background = backgrnd;
	if (cur_exp_lang == LANG_PDF && pdf_pagemode)
		page_job(&job);
	job.smooth = appres.smooth_factor > 1;
	job.quality = appres.jpeg_quality;
	stat = xq_svg_export(&job);
	unlink(svg);
	if (stat) {
		file_msg("%s", xq_svg_error());
		put_msg("Export to \"%s\" failed", file);
		return 1;
	}
	put_msg("Export to \"%s\" done (drawn from SVG: Ghostscript was not "
			"found)", file);
	return 0;
}

/* the print settings and fig2dev's SVG for printing without Ghostscript;
   the caller removes svg[PATH_MAX] */
int
xqb_print_svg(char *svg, struct xq_svg_job *job)
{
	static char	backgrnd[10];
	char		grid[80];
	char		dir[PATH_MAX];
	int		stat;

	if (check_action_on() || emptyfigure_msg("Print"))
		return -1;
	print_settings(backgrnd, grid);
	/* make_svg() writes picture paths relative to cur_export_dir */
	strcpy(dir, cur_export_dir);
	strcpy(cur_export_dir, cur_file_dir);
	change_directory(cur_file_dir);
	stat = make_svg(svg);
	strcpy(cur_export_dir, dir);
	if (stat)
		return -1;
	memset(job, 0, sizeof *job);
	job->svg = svg;
	job->dir = cur_file_dir;
	job->format = "pdf";
	job->mag = appres.magnification / 100.0;
	job->background = backgrnd;
	page_job(job);
	return 0;
}

/* gs_bitmap() for f_readeps.c: without Ghostscript, PDF pictures are
   rendered with QtPdf, at gs_bitmap()'s 160 pixels per inch */
int
xq_gs_bitmap(char *file, F_pic *pic, int llx, int lly, int urx, int ury)
{
	int		w, h;
	size_t		i, n;
	unsigned char	*rgb, *pos;

	if (have_ghostscript() || pic->pic_cache->subtype != T_PIC_PDF ||
			tool_cells <= 2 || appres.monochrome)
		return gs_bitmap(file, pic, llx, lly, urx, ury);

	w = (urx - llx) * 160 / 72 + 1;
	h = (ury - lly) * 160 / 72 + 1;
	n = (size_t)w * h;
	if ((rgb = malloc(n * 3)) == NULL)
		return -1;
	if (xq_pdf_raster(file, w, h, rgb)) {
		free(rgb);
		return -1;
	}
	pic->pic_cache->bit_size.x = w;
	pic->pic_cache->bit_size.y = h;
	if (tool_vclass == TrueColor && image_bpp == 4) {
		pos = pic->pic_cache->bitmap = malloc(n * 4);
		for (i = 0; pos && i < n; ++i, pos += 4)
			*(unsigned int *)pos = ((unsigned int)rgb[3*i] << 16) +
				((unsigned int)rgb[3*i+1] << 8) + rgb[3*i+2];
		pic->pic_cache->numcols = -1;
	} else {
		/* map_to_palette() takes BGR triples */
		pos = pic->pic_cache->bitmap = malloc(n * 3);
		for (i = 0; pos && i < n; ++i, pos += 3) {
			pos[0] = rgb[3*i+2];
			pos[1] = rgb[3*i+1];
			pos[2] = rgb[3*i];
		}
	}
	free(rgb);
	if (!pic->pic_cache->bitmap)
		return -1;
	if (pic->pic_cache->numcols != -1 && !map_to_palette(pic))
		return -1;
	return 0;
}

/* the mode panel's columns: -but_per_row, else xfig's default */
int
xqb_buttons_per_row(void)
{
	return appres.but_per_row > 0 ? appres.but_per_row : DEF_SW_PER_ROW;
}

/* ------------------------------------------- the picture's Browse button */

/*
 * The picture edit dialog's Browse... opens the system's file dialog; the
 * chosen file becomes the picture's file, as with xfig's browse panel.
 */
extern void	popup_browse_panel(Widget w);
static char	*(*native_browse)(const char *dir, const char *current);

void
xqb_set_native_browse(char *(*browse)(const char *dir, const char *current))
{
	native_browse = browse;
}

void
xq_popup_browse_panel(Widget w)
{
	char	*file, *abs_path, *cur;
	char	path_buf[PATH_MAX];
	char	*path = path_buf;

	if (!native_browse) {
		popup_browse_panel(w);
		return;
	}
	cur = internal_path(panel_get_value(pic_name_panel));
	file = native_browse(cur_file_dir, cur ? cur : "");
	free(cur);
	if (!file)
		return;
	abs_path = realpath(file, NULL);
	free(file);
	if (!abs_path)
		return;
	if (external_path(&path, sizeof path_buf, abs_path) >= 0)
		panel_set_value(pic_name_panel, path);
	free(abs_path);
	if (path != path_buf)
		free(path);
	/* reads the picture and shows it in the dialog */
	push_apply_button();
}
