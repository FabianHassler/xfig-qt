/*
 * FIG : Facility for Interactive Generation of figures
 * Copyright (c) 1985-1988 by Supoj Sutanthavibul
 * Parts Copyright (c) 1989-2007 by Brian V. Smith
 * Parts Copyright (c) 1991 by Paul King
 * Parts Copyright (c) 2016-2024 by Thomas Loimer
 *
 * Any party obtaining a copy of these files is granted, free of charge, a
 * full and unrestricted irrevocable, world-wide, paid up, royalty-free,
 * nonexclusive right and license to deal in this software and documentation
 * files (the "Software"), including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense and/or sell copies
 * of the Software, and to permit persons who receive copies from any such
 * party to do so, with the only requirement being that the above copyright
 * and this permission notice remain intact.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "w_canvas.h"

#include <inttypes.h>		/* SCNxLEAST32 */
#include <limits.h>		/* INT_MIN, INT_MAX */
#include <math.h>
#include <stdio.h>
#include <stdint.h>		/* uint_least32_t */
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "figx.h"
#include "resources.h"
#include "object.h"
#include "main.h"
#include "mode.h"
#include "paintop.h"
#include "d_text.h"
#include "e_edit.h"
#include "u_colors.h"
#include "u_create.h"
#include "u_pan.h"
#include "u_redraw.h"
#include "u_search.h"
#include "w_cmdpanel.h"
#include "w_cursor.h"
#include "w_drawprim.h"
#include "w_grid.h"
#include "w_icons.h"
#include "w_keyboard.h"
#include "w_layers.h"
#include "w_modepanel.h"
#include "w_mousefun.h"
#include "w_msgpanel.h"
#include "w_rulers.h"
#include "w_setup.h"
#include "w_snap.h"
#include "w_util.h"
#include "w_zoom.h"


/*********************** EXPORTS ************************/

void		(*canvas_kbd_proc) ();
void		(*canvas_locmove_proc) ();
void		(*canvas_ref_proc) ();
void		(*canvas_leftbut_proc) ();
void		(*canvas_middlebut_proc) ();
void		(*canvas_middlebut_save) ();
void		(*canvas_rightbut_proc) ();
void		(*return_proc) ();

int		clip_xmin, clip_ymin, clip_xmax, clip_ymax;
int		clip_width, clip_height;
int		cur_x, cur_y;
int		fix_x, fix_y;
int		last_x, last_y;		/* last position of mouse */
int		shift;			/* global state of shift key */
int		ignore_exp_cnt = 1;	/* we get 2 expose events at startup */
String		local_translations = "";


/*********************** LOCAL ************************/

typedef struct _CompKey	CompKey;
struct _CompKey {
	char		*str;
	unsigned char	first;
	unsigned char	second;
	CompKey		*next;
};
static CompKey		*allCompKey = NULL;
static void		readComposeKey(void);
static int		getComposeKey(char **str, char compose_buf[2]);

static void		canvas_paste(Widget w, XKeyEvent *paste_event);
static void		popup_mode_panel(Widget widget, XButtonEvent *event,
					String *params, Cardinal *num_params);
static void		popdown_mode_panel(void);


void
null_proc(void)
{
	/* almost does nothing */
	if (highlighting)
		erase_objecthighlight();
}

static void
canvas_exposed(Widget tool, XEvent *event, String *params, Cardinal *nparams)
{
	(void)tool;
	(void)params;
	(void)nparams;

	static int	xmin = 9999, xmax = -9999, ymin = 9999, ymax = -9999;
	XExposeEvent	*xe = (XExposeEvent *) event;
	register int	tmp;

	if (xe->x < xmin)
		xmin = xe->x;
	if (xe->y < ymin)
		ymin = xe->y;
	if ((tmp = xe->x + xe->width) > xmax)
		xmax = tmp;
	if ((tmp = xe->y + xe->height) > ymax)
		ymax = tmp;
	if (xe->count > 0)
		return;

	/* kludge to stop getting extra redraws at start up */
	if (ignore_exp_cnt)
		ignore_exp_cnt--;
	else
		redisplay_region(xmin, ymin, xmax, ymax);
	xmin = 9999, xmax = -9999, ymin = 9999, ymax = -9999;
}


static XtActionsRec	canvas_actions[] =
{
	{"EventCanv",		(XtActionProc)canvas_selected},
	{"ExposeCanv",		(XtActionProc)canvas_exposed},
	{"EnterCanv",		(XtActionProc)draw_mousefun_canvas},
	{"PasteCanv",		(XtActionProc)canvas_paste},
	{"LeaveCanv",		(XtActionProc)clear_mousefun},
	{"EraseRulerMark",	(XtActionProc)erase_rulermark},
	{"Unzoom",		(XtActionProc)unzoom},
	{"PanOrigin",		(XtActionProc)pan_origin},
	{"ToggleShowDepths",	(XtActionProc)toggle_show_depths},
	{"ToggleShowBalloons",	(XtActionProc)toggle_show_balloons},
	{"ToggleShowLengths",	(XtActionProc)toggle_show_lengths},
	{"ToggleShowVertexnums", (XtActionProc)toggle_show_vertexnums},
	{"ToggleShowBorders",	(XtActionProc)toggle_show_borders},
	{"ToggleAutoRefresh",	(XtActionProc)toggle_refresh_mode},
	{"PopupModePanel",	(XtActionProc)popup_mode_panel},
	{"PopdownModePanel",	(XtActionProc)popdown_mode_panel},
	{"PopupKeyboardPanel",	(XtActionProc)popup_keyboard_panel},
	{"PopdownKeyboardPanel", (XtActionProc)popdown_keyboard_panel},
};

/* need the ~Meta for the EventCanv action so that the accelerators still work
   during text input */
static String	canvas_translations =
   "<Motion>:EventCanv()\n\
    Any<BtnDown>:EventCanv()\n\
    Any<BtnUp>:EventCanv()\n\
    <Key>F18: PasteCanv()\n\
    <Key>F20: PasteCanv()\n\
    <EnterWindow>:EnterCanv()\n\
    <LeaveWindow>:LeaveCanv()EraseRulerMark()\n\
    <KeyUp>:EventCanv()\n\
    ~Meta<Key>:EventCanv()\n\
    <Expose>:ExposeCanv()\n";

void
init_canvas(Widget tool)
{
	DeclareArgs(12);

	FirstArg(XtNlabel, "");
	NextArg(XtNinternational, False);
	NextArg(XtNwidth, CANVAS_WD);
	NextArg(XtNheight, CANVAS_HT);
	NextArg(XtNborderWidth, 0);
	NextArg(XtNfromHoriz, mode_panel);
	NextArg(XtNfromVert, topruler_sw);
	NextArg(XtNtop, XtChainTop);
	NextArg(XtNleft, XtChainLeft);

	canvas_sw = XtCreateWidget("canvas", labelWidgetClass, tool,
			Args, ArgCount);
	canvas_leftbut_proc = null_proc;
	canvas_middlebut_proc = null_proc;
	canvas_rightbut_proc = null_proc;
	canvas_kbd_proc = canvas_locmove_proc = null_proc;
	XtAugmentTranslations(canvas_sw,
			XtParseTranslationTable(canvas_translations));
	readComposeKey();
}

void
add_canvas_actions(void)
{
	XtAppAddActions(tool_app, canvas_actions, XtNumber(canvas_actions));
}

/* at this point, the canvas widget is realized so we can get
   the window from it */

void setup_canvas(void)
{
	init_grid();
	reset_clip_window();
}

/*
 * some convenience functions to abridge canvas_selected() further below
 */

static Status
get_input(XKeyPressedEvent *kpe, char **buf, int buf_size, int *len,
		KeySym *key_sym)
{
	Status	status;
	if (xim_ic != NULL) {
		*len = Xutf8LookupString(xim_ic, kpe, *buf, buf_size, key_sym,
					&status);
		if (status == XLookupChars)
			key_sym = 0;
	} else {
		*len = XLookupString(kpe, *buf, buf_size, key_sym, NULL);
		if (*len > 0) {
			if (*key_sym == 0)
				status = XLookupChars;
			else
				status = XLookupBoth;
		} else {
			if (*key_sym == 0)
				status = XLookupNone;
			else
				status = XLookupKeySym;
		}
	}

	if (status == XBufferOverflow) {
		if (buf_size != *len) {
			*buf = malloc((size_t)len);
			status = get_input(kpe, buf, *len, len, key_sym);
		} else {
			/* we are in the second call to get_input() above */
			file_msg("Input buffer overflow, file %s, line %d.",
							__FILE__, __LINE__);
		}
	}

	return status;
}

static int
process_keystroke(XKeyPressedEvent *kpe, int *compose_key)
{
	int		len;
	Status		status;
	static char	compose_buf[2];
	char		b[8];
	char		*buf = b;
	size_t		buf_size = sizeof b / sizeof b[0];
	KeySym		key_sym;

	status = get_input(kpe, &buf, (int)buf_size, &len, &key_sym);

	if (status == XLookupNone) {
		if (buf != b)
			free(buf);
		return 1;
	}

	/* Provide for compose_key > 0, len == 0 because modifier keys (e.g.,
	   shift) generate an event and a key_sym, but do not return a string.*/
	if (*compose_key == 1 && len == 1) {
		/* first key of compose sequence */
		compose_buf[0] = buf[0];
		*compose_key = 2;

	} else if (*compose_key == 2 && len > 0) {
		if (len == 1) {
			/* second key of compose sequence */
			char	*str;
			compose_buf[1] = buf[0];
			if (!getComposeKey(&str, compose_buf)) {
				canvas_kbd_proc(str, (int)strlen(str),
								(KeySym)0);
			} else {
				canvas_kbd_proc(compose_buf, 1, (KeySym)0);
				canvas_kbd_proc(compose_buf + 1, 1, (KeySym)0);
			}
		} else {	/* len > 1 */
			canvas_kbd_proc(compose_buf, 1, (KeySym)0);
			canvas_kbd_proc(buf, len, key_sym);
		}
		setCompLED(0);
		*compose_key = 0;
	} else {
		canvas_kbd_proc(buf, len, key_sym);
		if (*compose_key == 1 && len > 1) {
			/* invalid first compose key */
			setCompLED(0);
			*compose_key = 0;
		}
	}
	if (buf != b)
		free(buf);
	return 0;
}

void
canvas_selected(Widget tool, XButtonEvent *event, String *params,
		Cardinal *nparams)
{
	(void)tool;
	(void)params;
	(void)nparams;

	KeySym			key;
	static int		sx = -10000, sy = -10000;
	XButtonPressedEvent	*be = (XButtonPressedEvent *)event;
	XKeyPressedEvent	*kpe = (XKeyPressedEvent *)event;
	Window			rw, cw;
	int			rx, ry, cx, cy;
	unsigned int		mask;
	int			x, y;
	static int		compose_key = 0;

	/* key on event type */
	switch (event->type) {


	case MotionNotify:

#if defined(SMOOTHMOTION)
		/* translate from zoomed coords to object coords */
		x = BACKX(event->x);
		y = BACKY(event->y);

		/* perform appropriate rounding if necessary */
		round_coords(&x, &y);

		if (x == sx && y == sy)
			return;
		sx = x;
		sy = y;
#else
		XQueryPointer(event->display, event->window,
				&rw, &cw,
				&rx, &ry,
				&cx, &cy,
				&mask);
		cx = BACKX(cx);
		cy = BACKY(cy);

		/* perform appropriate rounding if necessary */
		round_coords(&cx, &cy);

#ifdef REPORT_XY_ALWAYS
		put_msg("x = %.3f, y = %.3f %s",
				(float)cx/(appres.INCHES? PPI: PPCM),
				(float) cy/(appres.INCHES? PPI: PPCM),
				appres.INCHES? "in": "cm");
#endif

		if (cx == sx && cy == sy)
			break;

		/* draw crosshair cursor where pointer is */
		if (appres.crosshair && action_on) {
			pw_vector(canvas_win, 0, last_y,
					(int)(CANVAS_WD*ZOOM_FACTOR), last_y,
					INV_PAINT, 1, RUBBER_LINE, 0.0, RED);
			pw_vector(canvas_win, last_x, 0, last_x,
					(int)(CANVAS_HT*ZOOM_FACTOR),
					INV_PAINT, 1, RUBBER_LINE, 0.0, RED);
			pw_vector(canvas_win, 0, cy,
					(int)(CANVAS_WD*ZOOM_FACTOR), cy,
					INV_PAINT, 1, RUBBER_LINE, 0.0, RED);
			pw_vector(canvas_win, cx, 0, cx,
					(int)(CANVAS_HT*ZOOM_FACTOR),
					INV_PAINT, 1, RUBBER_LINE, 0.0, RED);
		}

		last_x = x = sx = cx;	/* these are zoomed */
		last_y = y = sy = cy;	/* coordinates!	    */

#endif /* SMOOTHMOTION */

		set_rulermark(x, y);
		(*canvas_locmove_proc) (x, y);
		break;


	case ButtonRelease:
		break;


	case ButtonPress:

		/* translate from zoomed coords to object coords */
		x = BACKX(event->x);
		y = BACKY(event->y);

		if ((SNAP_MODE_NONE != snap_mode) &&
				(be->button != Button3)) {
			/* these asssigments are here because x and y are
			   register vbls */
			int vx = x;
			int vy = y;

			/*
			 * if snap fails, i'm opting to punt entirely and let
			 * the user try again. could also just revert to the
			 * usual round_coords(), but i think that might cause
			 * confusion.
			 */

			if (snap_process(&vx, &vy, be->state & ShiftMask) ==
					False)
				break;
			else {
				x = vx;
				y = vy;
			}
		}
		else {
			/* if the point hasn't been snapped... */
			/* perform appropriate rounding if necessary */
			round_coords(&x, &y);
		}

		/* Convert Alt-Button3 to Button2 */
		if (be->button == Button3 && be->state & Mod1Mask) {
			be->button = Button2;
			be->state = be->state & ~Mod1Mask;
		}

		/* call interactive zoom function when only
		   control key pressed */
		if (!zoom_in_progress && ((be->state & ControlMask) &&
					!(be->state & ShiftMask))) {
			zoom_selected(x, y, be->button);
			break;
		}

		/* edit shape factor when pressing control & shift keys
		   in edit mode */
		if ((be->state & ControlMask && be->state & ShiftMask) &&
				cur_mode >= FIRST_EDIT_MODE) {
			change_sfactor(x, y, be->button);
			break;
		}

		if (be->button == Button1)
			/* left button proc */
			(*canvas_leftbut_proc) (x, y, be->state & ShiftMask);

		else if (be->button == Button2)
			/* middle button proc */
			(*canvas_middlebut_proc) (x, y, be->state & ShiftMask);

		else if (be->button == Button3)
			/* right button proc */
			(*canvas_rightbut_proc) (x, y, be->state & ShiftMask);

		else if (be->button == Button4 && !shift)
			/* pan down with wheelmouse */
			pan_down(0);

		else if (be->button == Button5 && !shift)
			/* pan up with wheelmouse */
			pan_up(0);

		else if ((be->button == 6 /* Button6 */ && !shift) ||
				(be->button == Button4 && shift))
			/* pan right with wheelmouse or touchpad */
			pan_right(0);

		else if ((be->button == 7 /* Button7 */ && !shift) ||
				(be->button == Button5 && shift))
			/* pan left with wheelmouse or touchpad */
			pan_left(0);

		break;


	case KeyPress:
	case KeyRelease:

		XQueryPointer(event->display, event->window,
				&rw, &cw, &rx, &ry, &cx, &cy, &mask);
		/* save global shift state */
		shift = mask & ShiftMask;

		if (True == keyboard_input_available) {
			XMotionEvent	me;

			if (keyboard_state & ShiftMask)
				(*canvas_middlebut_proc)(keyboard_x, keyboard_y,
					       0);
			else if (keyboard_state & ControlMask)
				(*canvas_rightbut_proc)(keyboard_x, keyboard_y,
						0);
			else
				(*canvas_leftbut_proc)(keyboard_x, keyboard_y,
						0);
			keyboard_input_available = False;

			/*
			 * get some sort of feedback, like a drawing rubberband,
			 * onto the canvas that the coord has been entered by
			 * faking up a motion event.
			 */

			last_x = x = sx = cx = keyboard_x;/* these are zoomed */
			last_y = y = sy = cy = keyboard_y;/* coordinates! */

			me.type			= MotionNotify;
			me.send_event		= 1;
			me.display		= event->display;
			me.window		= event->window;
			me.root			= event->root;
			me.subwindow		= event->subwindow;
			me.x			= event->x + 10;
			me.y			= event->y + 10;
			me.x_root		= event->x_root;
			me.y_root		= event->y_root;
			me.state		= event->state;
			me.is_hint		= 0;
			me.same_screen		= event->same_screen;;
			XtDispatchEvent((XEvent *)(&me));

			break;
		}

		/* we might want to check action_on */
		/* if arrow keys are pressed, pan */
		key = XLookupKeysym(kpe, 0);

		/* do the mouse function stuff first */
		if (zoom_in_progress) {
			set_temp_cursor(magnify_cursor);
			draw_mousefun("final point", "", "cancel");
		} else if (mask & ControlMask) {
			if (mask & ShiftMask) {
				reset_cursor();
				if (cur_mode >= FIRST_EDIT_MODE)
					draw_mousefun("More approx",
							"Cycle shapes",
							"More interp");
				else
					draw_shift_mousefun_canvas();
			} else {  /* show control-key action */
				set_temp_cursor(magnify_cursor);
				draw_mousefun("Zoom area", "Pan to origin",
						"Unzoom");
			}
		} else if (mask & ShiftMask) {
			reset_cursor();
			draw_shift_mousefun_canvas();
		} else {
			reset_cursor();
			draw_mousefun_canvas();
		}

		if (event->type == KeyPress) {

			/* pan the canvas */
			if (key == XK_Up || key == XK_Down ||
					/* don't process the following if
					   in text input mode */
					((key == XK_Left || key == XK_Right ||
					  key == XK_Home) &&
					 (!action_on || cur_mode != F_TEXT))) {
				switch (key) {
				case XK_Left:
					pan_right(event->state&ShiftMask);
					break;
				case XK_Right:
					pan_left(event->state&ShiftMask);
					break;
				case XK_Up:
					pan_down(event->state&ShiftMask);
					break;
				case XK_Down:
					pan_up(event->state&ShiftMask);
					break;
				case XK_Home:
					pan_origin();
					break;
				} /* switch (key) */

			/* detect the compose key */
			} else if (allCompKey && (key == XK_Multi_key ||
						key == XK_Meta_L ||
						key == XK_Meta_R ||
						key == XK_Alt_L ||
						key == XK_Alt_R) && action_on
					&& cur_mode == F_TEXT) {
				compose_key = 1;
				setCompLED(1);
				break;

			/* edit text */
			} else {
				if (canvas_kbd_proc != null_proc ) {
					if (key == XK_Left || key == XK_Right ||
							key == XK_Home ||
							key == XK_End) {
						if (compose_key) {
							/* in case Meta was
							   followed by
							   cursor movement */
							setCompLED(0);
							compose_key = 0;
						}
						canvas_kbd_proc(NULL, 0, key);
					} else {
						if (process_keystroke(kpe,
								&compose_key))
							break;
					}
				} else {
					/*
					 * Be cheeky... we aren't going to do
					 * anything, so pass the key on to the
					 * mode_panel window by rescheduling the
					 * event
					 * The message window might treat it as
					 * a hotkey!
					 */
					kpe->window = XtWindow(mode_panel);
					kpe->subwindow = 0;
					XPutBackEvent(kpe->display,
							(XEvent *)kpe);
				}
			}
			break;
		} /* event-type == KeyPress */
	} /* switch(event->type) */
}

/* clear the canvas - this can't be called to clear a pixmap, only a window */

void
clear_canvas(void)
{
	/* clear the splash graphic if it is still on the screen */
	if (splash_onscreen) {
		splash_onscreen = False;
		XClearArea(tool_d, canvas_win, 0, 0, CANVAS_WD, CANVAS_HT,
				False);
	} else {
		XClearArea(tool_d, canvas_win, clip_xmin, clip_ymin,
				clip_width, clip_height, False);
	}
	/* redraw any page border */
	redisplay_pageborder();
}

void
clear_region(int xmin, int ymin, int xmax, int ymax)
{
	XClearArea(tool_d, canvas_win, xmin, ymin,
			xmax - xmin + 1, ymax - ymin + 1, False);
}

static void
get_canvas_clipboard(Widget w, XtPointer client_data, Atom *selection,
		Atom *type, XtPointer buf, unsigned long *length, int *format)
{
	(void)client_data;
	(void)selection;
	char		**tmp;
	int		i, num_values, ret_status;
	XTextProperty	prop;
	Atom		atom_utf8_string;

	atom_utf8_string = XInternAtom(XtDisplay(w), "UTF8_STRING", False);

	if (*type == atom_utf8_string) {
		prop.value = buf;
		prop.encoding = *type;
		prop.format = *format;
		prop.nitems = *length;
		num_values = 0;
		ret_status = Xutf8TextPropertyToTextList(XtDisplay(w), &prop,
				&tmp, &num_values);
		if (ret_status == Success || 0 < num_values) {
			for (i = 0; i < num_values; ++i) {
				canvas_kbd_proc((unsigned char *)tmp[i],
						(int)strlen(tmp[i]), (KeySym)0);
			}
		}
	} else {
		canvas_kbd_proc((unsigned char *)buf, (int)(*length),(KeySym)0);
	}
	XtFree(buf);
}

/* paste primary X selection to the canvas */

void
paste_primary_selection(void)
{
	/* turn off Compose key LED */
	setCompLED(0);
	canvas_paste(canvas_sw, NULL);
}

static void
canvas_paste(Widget w, XKeyEvent *paste_event)
{
	Time event_time;
	Atom	atom_utf8_string;

	if (canvas_kbd_proc != (void (*)())char_handler)
		return;

	if (paste_event != NULL)
		event_time = paste_event->time;
	else
		event_time = CurrentTime;

	atom_utf8_string = XInternAtom(XtDisplay(w), "UTF8_STRING", False);
	if (atom_utf8_string)
		XtGetSelectionValue(w, XA_PRIMARY, atom_utf8_string,
				get_canvas_clipboard, NULL, event_time);
	else
		XtGetSelectionValue(w, XA_PRIMARY, XA_STRING,
				get_canvas_clipboard, NULL, event_time);
}

static int
getComposeKey(char **str, char compose_buf[2])
{
	CompKey	   *compKeyPtr = allCompKey;

	while (compKeyPtr != NULL) {
		if (compKeyPtr->first == (unsigned char)compose_buf[0] &&
				compKeyPtr->second ==
					(unsigned char)compose_buf[1]) {
			*str = compKeyPtr->str;
			return 0;
		} else {
			compKeyPtr = compKeyPtr->next;
		}
	}
	return -1;
}

static void
readComposeKey(void)
{
	FILE		*st;
	CompKey		*compKeyPtr;
	char		line[255];
	char		*p;
	char		*p1;
	char		*p2;
	char		*p3;
	long		size;


	/* Treat the compose key DB a different way.  In this order:
	 *
	 *  1.	If the resource contains no "/", prefix it with the name of
	 *	the wired-in directory and use that.
	 *
	 *  2.	Otherwise see if it begins with "~/", and if so use that,
	 *	with the leading "~" replaced by the name of this user's
	 *	$HOME directory.
	 *
	 * This way a user can have private compose key settings even when
	 * running xfig privately.
	 *
	 * Pete Kaiser
	 * 24 April 1992
	 */

	/* no / in name, make relative to XFIGLIBDIR */
	if (strchr(appres.keyFile, '/') == NULL) {
		strcpy(line, XFIGLIBDIR);
		strcat(line, "/");
		strcat(line, appres.keyFile);
	}

	/* expand the ~ to the user's home directory */
	else if (!strncmp(appres.keyFile, "~/", 2)) {
		strcpy(line, getenv("HOME"));
		strcat(line, appres.keyFile + 1);
	} else
		strcpy(line, appres.keyFile);

	if ((st = fopen(line, "r")) == NULL) {
		file_msg("Unable to open compose key file '%s',", line);
		file_msg("\tno multi-key sequences available.");
		return;
	}
	fseek(st, 0L, SEEK_END);
	size = ftell(st);
	fseek(st, 0L, SEEK_SET);

	local_translations = (String) new_string(size);

	strcpy(local_translations, "");
	while (fgets(line, 250, st) != NULL) {
		if (line[0] == '#')
			continue;
		strcat(local_translations, line);
		if ((p = strstr(line, "Multi_key")) != NULL) {
			char		buf[FC_UTF8_MAX_LEN + 1];

			if ((p1 = strstr(p, "<Key>")) == NULL)
				continue;
			p1 += strlen("<Key>");
			if ((p = strstr(p1, ",")) == NULL)
				continue;
			*p++ = '\0';
			if ((p2 = strstr(p, "<Key>")) == NULL)
				continue;
			p2 += strlen("<Key>");
			if ((p = strstr(p2, ":")) == NULL)
				continue;
			*p++ = '\0';
			if ((p3 = strstr(p, "insert-string(")) == NULL)
				continue;
			p3 +=strlen("insert-string(");
			if ((p = strstr(p3, ")")) == NULL)
				continue;
			*p++ = '\0';

			if (!strncmp(p3, "0x", 2)) {
				int			l;
				uint_least32_t	x;
				sscanf(p3, "%" SCNxLEAST32, &x);
				l = FcUcs4ToUtf8((FcChar32)x, (FcChar8 *)buf);
				buf[l] = '\0';
				p3 = buf;
			}

			if (allCompKey == NULL) {
				allCompKey = malloc(sizeof(CompKey));
				compKeyPtr = allCompKey;
			} else {
				compKeyPtr->next = malloc(sizeof(CompKey));
				compKeyPtr = compKeyPtr->next;
			}
			compKeyPtr->str = strdup(p3);
			compKeyPtr->first = XStringToKeysym(p1);
			compKeyPtr->second = XStringToKeysym(p2);
			compKeyPtr->next = NULL;
		}
	}
	fclose(st);
	if (allCompKey == NULL)
		file_msg("Empty compose key database, "
				"no compose sequences available.");
}

void
setCompLED(int on)
{
#ifdef COMP_LED
	XKeyboardControl values;
	values.led = COMP_LED;
	values.led_mode = on ? LedModeOn : LedModeOff;
	XChangeKeyboardControl(tool_d, KBLed|KBLedMode, &values);
#endif /* COMP_LED */
}

/* toggle the length lines when drawing or moving points */
void
toggle_show_lengths(void)
{
	appres.showlengths = !appres.showlengths;
	put_msg("%s lengths of lines in red ",
			appres.showlengths ? "Show" : "Don't show");
	refresh_view_menu();
}

/* toggle the drawing of vertex numbers on objects */
void
toggle_show_vertexnums(void)
{
	appres.shownums = !appres.shownums;
	put_msg("%s vertex numbers on objects",
			appres.shownums ? "Show" : "Don't show");
	refresh_view_menu();

	/* if user just turned on vertex numbers, redraw only objects */
	if (appres.shownums) {
		redisplay_canvas();
	} else {
		/* otherwise redraw whole canvas */
		clear_canvas();
		redisplay_canvas();
	}
}

/* toggle the drawing of page borders on the canvas */
void
toggle_show_borders(void)
{
	appres.show_pageborder = !appres.show_pageborder;
	put_msg("%s page borders on canvas",
			appres.show_pageborder ? "Show" : "Don't show");
	refresh_view_menu();

	/* if user just turned on the border, draw only it */
	if (appres.show_pageborder) {
		redisplay_pageborder();
	} else {
		/* otherwise redraw whole canvas */
		clear_canvas();
		redisplay_canvas();
	}
}

/* toggle the information balloons */
void
toggle_show_balloons(void)
{
	appres.showballoons = !appres.showballoons;
	put_msg("%s information balloons",
			appres.showballoons ? "Show" : "Don't show");
	refresh_view_menu();
}



/* popup drawing/editing mode panel on the canvas.
  This can be useful especially if wheel-mouse is in use.  - T.Sato */

static Widget draw_panel = None;
static Widget edit_panel = None;
static Widget active_mode_panel = None;

extern mode_sw_info mode_switches[];

static void
popdown_mode_panel(void)
{
	if (active_mode_panel != None) XtPopdown(active_mode_panel);
	active_mode_panel = None;
}

static void
mode_panel_button_selected(Widget w, icon_struct *icon, char *call_data)
{
	(void)w;
	(void)call_data;

	change_mode(icon);
	popdown_mode_panel();
}

static void
create_mode_panel(void)
{
	Widget		draw_form, edit_form;
	Widget		form, entry;
	icon_struct	*icon;
	Widget		up = None, left = None;
	int		max_wd = 150, wd = 0;
	int		i;

	draw_panel = XtVaCreatePopupShell("draw_menu",
			transientShellWidgetClass, tool,
			XtNtitle, "Drawing Modes",
			XtNtitleEncoding, XA_STRING, NULL);
	draw_form = XtVaCreateManagedWidget("form", formWidgetClass, draw_panel,
			XtNdefaultDistance, 0, NULL);

	edit_panel = XtVaCreatePopupShell("edit_menu",
			transientShellWidgetClass, tool,
			XtNtitle, "Editing Modes",
			XtNtitleEncoding, XA_STRING, NULL);
	edit_form = XtVaCreateManagedWidget("form", formWidgetClass, edit_panel,
			XtNdefaultDistance, 0, NULL);

	form = draw_form;
	for (i = 0; mode_switches[i].mode != F_NULL; i++) {
		if (form == draw_form &&
				FIRST_EDIT_MODE <= mode_switches[i].mode) {
			form = edit_form;
			left = None;
			up = None;
			wd = 0;
		}
		icon = mode_switches[i].icon;
		wd = wd + icon->width;
		if (max_wd < wd) {
			up = left;
			left = None;
			wd = icon->width;
		}
		entry = XtVaCreateManagedWidget("button", commandWidgetClass,
				form, XtNlabel, "", XtNinternational, False,
				XtNresizable, False,
				XtNtop, XawChainTop, XtNbottom, XawChainTop,
				XtNleft, XawChainLeft, XtNright, XawChainLeft,
				XtNwidth, icon->width, XtNheight, icon->height,
				XtNbackgroundPixmap, mode_switches[i].pixmap,
				NULL);
		if (up != None)
			XtVaSetValues(entry, XtNfromVert, up, NULL);
		if (left != None)
			XtVaSetValues(entry, XtNfromHoriz, left, NULL);
		XtAddCallback(entry, XtNcallback,
				(XtCallbackProc)mode_panel_button_selected,
				(XtPointer)mode_switches[i].icon);
		left = entry;
	}
}

static void
popup_mode_panel(Widget widget, XButtonEvent *event, String *params,
		Cardinal *num_params)
{
	(void)widget;
	(void)num_params;

	Dimension wd, ht;
	Widget panel;

	if (draw_panel == None) {
		create_mode_panel();
		XtRealizeWidget(draw_panel);
		XtRealizeWidget(edit_panel);
		XSetWMProtocols(tool_d, XtWindow(draw_panel), &wm_delete_window,
				1);
		XSetWMProtocols(tool_d, XtWindow(edit_panel), &wm_delete_window,
				1);
	}
	panel = (strcmp(params[0], "edit") == 0) ? edit_panel : draw_panel;
	if (active_mode_panel != panel)
		popdown_mode_panel();
	XtVaGetValues(panel, XtNwidth, &wd, XtNheight, &ht, NULL);
	XtVaSetValues(panel, XtNx, event->x_root - wd / 2,
			XtNy, event->y_root - ht * 2 / 3, NULL);
	XtPopup(panel, XtGrabNone);
	active_mode_panel = panel;
}

/* round coordinate on square grid */
static void
round_square(int *x, const int spacing, const int half)
{
	if (*x < INT_MIN + half + 1) {
		*x = -(INT_MAX / spacing) * spacing;
	} else if (*x >= INT_MAX - half) {
		*x = (INT_MAX / spacing) * spacing;
	} else {
		int d = *x % spacing;

		/*
		 * The standard guarantees that
		 *   (a/b)*b + a%b == a.
		 * But, for a < 0, it is implementation defined whether, e.g.,
		 *   -7 / 3 == -2  &&  -7 % 3 == -1, or
		 *   -7 / 3 == -3  &&  -7 % 3 == 2.
		 */
		if (d > 0)
			*x += d < half ? -d : -d + spacing;
		else if (d < 0)
			*x += d < -half ? -d - spacing : -d;
	}
}


int
point_spacing(void)
{
	int	grid_unit;
	int	spacing;

	grid_unit = (appres.userscale != 1.0 && appres.INCHES) ?
		TENTH_UNIT : cur_gridunit;
	spacing = (int)(posn_rnd[grid_unit][cur_pointposn] / appres.userscale);
	return spacing;
}

/* macro which rounds coordinates depending on point positioning mode */
void
round_coords(int *restrict x, int *restrict y)
{
	const int	spacing = point_spacing();
	const int	half = spacing / 2;

#define	ROUNDING_UNNECESSARY	cur_pointposn == P_ANY || anypointposn
	/* make sure the cursor is on grid */
	if (ROUNDING_UNNECESSARY || spacing < 2)
		return;

	if (cur_gridtype == GRID_ISO) {
		/* do all calculations in double */
		double xd = *x;
		double yd = *y;
		const double c30 = sqrt(0.75);		/* cos(30) */
		double txx;		/* position from nearest whole grid */
		const double s = spacing;		/* whole grid interval */
		const double h = half;			/* half grid interval */

		/* determine x */
		xd = ( fabs( txx = fmod( xd, s * c30 ) ) < ( h * c30 ) ) ?
			xd - txx : xd + ( xd >= 0 ? s * c30 : -s * c30 ) - txx;

		/* determine y depending on x */
		if( fabs( fmod( xd / ( s * c30 ), 2.0 ) ) > 0.5 ) {
			yd = ( fabs( txx = fmod( yd + h, s ) ) < h ) ?
				yd - txx : yd + ( yd >= 0 ? s : -s ) - txx;
		} else {
			yd = ( fabs( txx = fmod( yd, s ) ) < h ) ?
				yd - txx : yd + ( yd >= 0 ? s : -s ) - txx;
		}

		/* check for overflow */
		if (xd < INT_MIN || xd > INT_MAX ||
				yd < INT_MIN || yd > INT_MAX) {
			if (xd < INT_MIN)
				*x = xd + s * c30;
			else if (xd > INT_MAX)
				*x = xd - s * c30;
			if (yd < INT_MIN)
				*y = yd + s; /* h gave segfault */
			else if  (yd > INT_MAX)
				*y = yd - s; /* h gave segfault */
			round_coords(x, y);
		} else {
			/* store (return) result as integer */
			*x = xd;
			*y = yd;
		}
	} else {	/* cur_gridtype == GRID_SQUARE */
		round_square(x, spacing, half);
		round_square(y, spacing, half);
	}
}

void
floor_coords(int *restrict x, int *restrict y)
{
	int	x_orig = *x;
	int	y_orig = *y;
	int	x_spacing;
	int	y_spacing;

	if (ROUNDING_UNNECESSARY)
		return;

	if (cur_gridtype == GRID_ISO) {
		y_spacing = point_spacing();
		x_spacing = y_spacing * sqrt(0.75);
	} else {
		x_spacing = y_spacing = point_spacing();
	}

	round_coords(x, y);

	/* for isometric grid, first round in x-direction; depending on
	   the phase, points in y-direction may change by half a spacing */
	if (x_orig < *x && *x >= INT_MIN + x_spacing) {
		*x -= x_spacing;
		*y = y_orig;
		round_coords(x, y);
	}
	if (y_orig < *y && *y >= INT_MIN + y_spacing)
		*y -= y_spacing;
}

void
ceil_coords(int *restrict x, int *restrict y)
{
	int	x_orig = *x;
	int	y_orig = *y;
	int	x_spacing;
	int	y_spacing;

	if (ROUNDING_UNNECESSARY)
		return;
#undef	ROUNDING_UNNECESSARY

	if (cur_gridtype == GRID_ISO) {
		y_spacing = point_spacing();
		x_spacing = y_spacing * sqrt(0.75);
	} else {
		x_spacing = y_spacing = point_spacing();
	}

	round_coords(x, y);

	/* for isometric grid, first round in x-direction; depending on
	   the phase, points in y-direction may change by half a spacing */
	if (x_orig > *x && *x <= INT_MAX - x_spacing) {
		*x += x_spacing;
		*y = y_orig;
		round_coords(x, y);
	}
	if (y_orig > *y && *y <= INT_MAX - y_spacing)
		*y += y_spacing;
}
