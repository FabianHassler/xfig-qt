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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "w_layers.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <X11/Intrinsic.h>     /* includes X11/Xlib.h, which includes X11/X.h */
#include <X11/Shell.h>
#include <X11/StringDefs.h>

#include "figx.h"
#include "resources.h"
#include "object.h"
#include "mode.h"
#include "f_read.h"
#include "u_redraw.h"
#include "w_drawprim.h"
#include "w_canvas.h"
#include "w_cmdpanel.h"
#include "w_file.h"
#include "w_indpanel.h"
#include "w_layers.h"
#include "w_msgpanel.h"
#include "w_setup.h"
#include "w_snap.h"
#include "w_util.h"


/* EXPORTS */

/* which layer buttons are active */
Boolean	active_layers[MAX_DEPTH +1];
Boolean	any_active_in_compound(F_compound *cmpnd);
Widget	layer_form;
Boolean	gray_layers = True;
int	object_depths[MAX_DEPTH +1];	/* count of objects at each depth */
int	saved_depths[MAX_DEPTH +1];	/* saved when delete all is done */
int	saved_min_depth, saved_max_depth; /* saved min/max depth */
int	max_depth_request, min_depth_request;
Boolean	save_layers[MAX_DEPTH+1];
int	LAYER_WD=DEF_LAYER_WD;
int	LAYER_HT;

/* LOCALS */

DeclareStaticArgs(20);

static	Widget		all_active_but, all_inactive_but, toggle_all_but;
static	Widget		graytoggle, blanktoggle, graylabel, blanklabel;
static	Widget		layer_viewp, layer_canvas;
static	void	toggle_layer(Widget w, XButtonEvent *event, String *params, Cardinal *nparams);
static	void	set_depth_to_layer(Widget w, XButtonEvent *event, String *params, Cardinal *nparams);
static	void	sweep_layer(Widget w, XButtonEvent *event, String *params, Cardinal *nparams);
static	void	leave_layer(Widget w, XButtonEvent *event, String *params, Cardinal *nparams);
static	void	all_active(Widget w, XtPointer closure, XtPointer call_data);
static	void	all_inactive(Widget w, XtPointer closure, XtPointer call_data);
static	void	toggle_all(Widget w, XtPointer closure, XtPointer call_data);
static	void	switch_layer_mode(Widget w, XtPointer closure, XtPointer call_data);
static	void	layer_exposed(Widget w, XExposeEvent *event, String *params, Cardinal *nparams);
static  void	draw_layer_button(Window win, int but);
static  void	draw_layer_buttons(void);

XtActionsRec	layer_actions[] =
{
    {"ExposeLayerButtons", (XtActionProc) layer_exposed},
    {"ToggleLayerButton", (XtActionProc) toggle_layer},
    {"SetDepthToLayer", (XtActionProc) set_depth_to_layer},
    {"SweepLayerButton", (XtActionProc) sweep_layer},
    {"LeaveLayerButton", (XtActionProc) leave_layer},
};

static String	layer_translations =
    "<Btn1Down>:ToggleLayerButton()\n\
     <Btn1Motion>: SweepLayerButton()\n\
     <Btn3Down>:SetDepthToLayer()\n\
     <Leave>: LeaveLayerButton()\n\
     <Expose>:ExposeLayerButtons()\n";



void
reset_layers(void)
{
  int i;
  for (i=0; i<=MAX_DEPTH;  i++) {
     active_layers[i] = True;
  }
}

void
reset_depths(void)
{
  int i;
  for (i=0; i<=MAX_DEPTH;  i++) {
     object_depths[i] = 0;
  }
}

void
init_depth_panel(Widget parent)
{
    Widget	 label, below;
    Widget	 layer_viewform;

    /* MOUSEFUN_HT and ind_panel height aren't known yet */
    LAYER_HT = TOPRULER_HT + CANVAS_HT;

    /* main form to hold all the layer stuff */

    FirstArg(XtNfromHoriz, sideruler_sw);
    NextArg(XtNdefaultDistance, 1);
    NextArg(XtNwidth, LAYER_WD);
    NextArg(XtNheight, LAYER_HT);
    NextArg(XtNleft, XtChainRight);
    NextArg(XtNright, XtChainRight);
    NextArg(XtNfromVert, snap_indicator_panel);
    layer_form = XtCreateWidget("layer_form",formWidgetClass, parent, Args, ArgCount);
    if (appres.showdepthmanager)
	XtManageChild(layer_form);

    /* a label */
    FirstArg(XtNlabel, "Depths   ");
    NextArg(XtNinternational, False);
    NextArg(XtNtop, XtChainTop);
    NextArg(XtNbottom, XtChainTop);
    NextArg(XtNleft, XtChainLeft);
    NextArg(XtNright, XtChainRight);
    label = XtCreateManagedWidget("layer_label", labelWidgetClass, layer_form,
				Args, ArgCount);

    /* buttons to make all active, all inactive or toggle all */

    FirstArg(XtNlabel, "All On  ");
    NextArg(XtNinternational, False);
    NextArg(XtNfromVert, label);
    NextArg(XtNtop, XtChainTop);
    NextArg(XtNbottom, XtChainTop);
    NextArg(XtNleft, XtChainLeft);
    NextArg(XtNright, XtChainRight);
    below = all_active_but = XtCreateManagedWidget("all_active", commandWidgetClass,
				layer_form, Args, ArgCount);
    XtAddCallback(below, XtNcallback, (XtCallbackProc)all_active, (XtPointer) NULL);

    FirstArg(XtNlabel, "All Off ");
    NextArg(XtNinternational, False);
    NextArg(XtNfromVert, below);
    NextArg(XtNtop, XtChainTop);
    NextArg(XtNbottom, XtChainTop);
    NextArg(XtNleft, XtChainLeft);
    NextArg(XtNright, XtChainRight);
    below = all_inactive_but = XtCreateManagedWidget("all_inactive", commandWidgetClass,
				layer_form, Args, ArgCount);
    XtAddCallback(below, XtNcallback, (XtCallbackProc)all_inactive, (XtPointer) NULL);

    FirstArg(XtNlabel, "Toggle  ");
    NextArg(XtNinternational, False);
    NextArg(XtNfromVert, below);
    NextArg(XtNtop, XtChainTop);
    NextArg(XtNbottom, XtChainTop);
    NextArg(XtNleft, XtChainLeft);
    NextArg(XtNright, XtChainRight);
    below = toggle_all_but = XtCreateManagedWidget("toggle_all", commandWidgetClass,
				layer_form, Args, ArgCount);
    XtAddCallback(below, XtNcallback, (XtCallbackProc)toggle_all, (XtPointer) NULL);

    /* radio buttons to switch between graying out inactive layers or blanking altogether */

    /* radio for graying out */
    /* we'll insert the checkmark bitmap in setup_depth_panel */
    FirstArg(XtNwidth, sm_check_width+6);
    NextArg(XtNheight, sm_check_height+6);
    NextArg(XtNfromVert, below);
    NextArg(XtNtop, XtChainTop);
    NextArg(XtNbottom, XtChainTop);
    NextArg(XtNleft, XtChainLeft);
    NextArg(XtNright, XtChainLeft);
    NextArg(XtNinternalWidth, 1);
    NextArg(XtNinternalHeight, 1);
    NextArg(XtNlabel, " ");
    NextArg(XtNinternational, False);
    NextArg(XtNsensitive, (gray_layers? False : True));	/* make opposite button sens */
    NextArg(XtNstate, gray_layers);	/* initial state */
    NextArg(XtNradioData, 1);		/* when this is pressed the value is 1 */
    graytoggle = XtCreateManagedWidget("graytoggle", toggleWidgetClass,
				layer_form, Args, ArgCount);
    XtAddCallback(graytoggle, XtNcallback, (XtCallbackProc) switch_layer_mode,
					(XtPointer) NULL);

    /* label - "Gray" */

    FirstArg(XtNborderWidth, 0);
    NextArg(XtNfromVert, below);
    NextArg(XtNfromHoriz, graytoggle);
    NextArg(XtNtop, XtChainTop);
    NextArg(XtNbottom, XtChainTop);
    NextArg(XtNleft, XtChainRight);
    NextArg(XtNright, XtChainRight);
    NextArg(XtNlabel, "Gray  ");
    NextArg(XtNinternational, False);
    graylabel = below = XtCreateManagedWidget("graylabel", labelWidgetClass,
				layer_form, Args, ArgCount);

    /* radio for blanking */
    FirstArg(XtNwidth, sm_check_width+6);
    NextArg(XtNheight, sm_check_height+6);
    NextArg(XtNfromVert, graytoggle);
    NextArg(XtNtop, XtChainTop);
    NextArg(XtNbottom, XtChainTop);
    NextArg(XtNleft, XtChainLeft);
    NextArg(XtNright, XtChainLeft);
    NextArg(XtNinternalWidth, 1);
    NextArg(XtNinternalHeight, 1);
    NextArg(XtNlabel, " ");
    NextArg(XtNinternational, False);
    NextArg(XtNsensitive, (gray_layers? True : False));	/* make opposite button sens */
    NextArg(XtNstate, !gray_layers);	/* initial state */
    NextArg(XtNradioData, 2);		/* when this is pressed the value is 2 */
    NextArg(XtNradioGroup, graytoggle);	/* this is the other radio button in the group */
    blanktoggle = XtCreateManagedWidget("blanktoggle", toggleWidgetClass,
				layer_form, Args, ArgCount);
    XtAddCallback(blanktoggle, XtNcallback, (XtCallbackProc) switch_layer_mode,
					(XtPointer) NULL);

    /* label - "Blank" */

    FirstArg(XtNborderWidth, 0);
    NextArg(XtNfromVert, graytoggle);
    NextArg(XtNfromHoriz, blanktoggle);
    NextArg(XtNlabel, "Blank ");
    NextArg(XtNinternational, False);
    NextArg(XtNtop, XtChainTop);
    NextArg(XtNbottom, XtChainTop);
    NextArg(XtNleft, XtChainRight);
    NextArg(XtNright, XtChainRight);
    blanklabel = below = XtCreateManagedWidget("blanklabel", labelWidgetClass,
				layer_form, Args, ArgCount);

    /* a form to hold the "Front" and "Back" labels and the viewport widget */

    FirstArg(XtNborderWidth, 1);
    NextArg(XtNfromVert, below);
    NextArg(XtNdefaultDistance, 1);
    NextArg(XtNtop, XtChainTop);
    NextArg(XtNbottom, XtChainBottom);
    NextArg(XtNleft, XtChainRight);
    NextArg(XtNright, XtChainRight);
    layer_viewform = XtCreateManagedWidget("layer_viewform",formWidgetClass,
			layer_form, Args, ArgCount);

    /* label - "Front" */

    FirstArg(XtNborderWidth, 0);
    NextArg(XtNlabel, " Front ");
    NextArg(XtNinternational, False);
    NextArg(XtNtop, XtChainTop);
    NextArg(XtNbottom, XtChainTop);
    NextArg(XtNleft, XtChainRight);
    NextArg(XtNright, XtChainRight);
    below = XtCreateManagedWidget("frontlabel", labelWidgetClass,
				layer_viewform, Args, ArgCount);

    /* viewport to be able to scroll the layer buttons */

    FirstArg(XtNborderWidth, 1);
    NextArg(XtNwidth, LAYER_WD);
    NextArg(XtNallowVert, True);
    NextArg(XtNfromVert, below);
    NextArg(XtNtop, XtChainTop);
    NextArg(XtNbottom, XtChainBottom);
    NextArg(XtNleft, XtChainLeft);
    NextArg(XtNright, XtChainRight);
    layer_viewp = XtCreateManagedWidget("layer_viewp", viewportWidgetClass,
			layer_viewform, Args, ArgCount);
    /* popup when mouse passes over button */

    /* pass null pointer for trigger message so we'll use the two-line one */
    XtAddEventHandler(layer_viewp, StructureNotifyMask, False,
			   (XtEventHandler)update_layers, (XtPointer) NULL);

    /* canvas (label, actually) in which to create the buttons */
    /* we're not using real commandButtons because of the time to create
       potentially hundreds of them depending on the number of layers in the figure */
    FirstArg(XtNleft, XtChainLeft);
    NextArg(XtNright, XtChainRight);
    NextArg(XtNlabel, "");
    NextArg(XtNinternational, False);
    layer_canvas = XtCreateManagedWidget("layer_canvas", labelWidgetClass,
				layer_viewp, Args, ArgCount);
    /* label - "Back" */

    FirstArg(XtNborderWidth, 0);
    NextArg(XtNfromVert, layer_viewp);
    NextArg(XtNlabel, " Back  ");
    NextArg(XtNinternational, False);
    NextArg(XtNtop, XtChainBottom);
    NextArg(XtNbottom, XtChainBottom);
    NextArg(XtNleft, XtChainRight);
    NextArg(XtNright, XtChainRight);
    below = XtCreateManagedWidget("backlabel", labelWidgetClass,
				layer_viewform, Args, ArgCount);

    /* make actions/translations for user to click on a layer "button" and
       to redraw buttons on expose */
    XtAppAddActions(tool_app, layer_actions, XtNumber(layer_actions));
    XtOverrideTranslations(layer_canvas,
			   XtParseTranslationTable(layer_translations));

    update_layerpanel();
}

/* now that the mouse function and indicator panels have been sized
   correctly, resize our form */

void
setup_depth_panel(void)
{
    Dimension	 ind_ht, snap_ht=0;
    /* get height of indicator and snap panels */
    FirstArg(XtNheight, &ind_ht);
    GetValues(ind_panel);
    FirstArg(XtNheight, &snap_ht);
    GetValues(snap_indicator_panel);
    LAYER_HT = MOUSEFUN_HT + TOPRULER_HT + CANVAS_HT + ind_ht - snap_ht + INTERNAL_BW*4;

    /* now that the bitmaps have been created, put the checkmark in the proper toggle */
    FirstArg(XtNbitmap, (gray_layers? sm_check_pm : sm_null_check_pm));
    SetValues(graytoggle);
    FirstArg(XtNbitmap, (gray_layers? sm_null_check_pm : sm_check_pm));
    SetValues(blanktoggle);

    XtUnmanageChild(layer_form);
    FirstArg(XtNheight, LAYER_HT);
    SetValues(layer_form);
    XtManageChild(layer_form);
}

#define B_WIDTH		48
#define B_BORDER	2
#define C_SIZE		10
#define TOT_B_HEIGHT	(C_SIZE+2*B_BORDER)

/* now make a checkbox for each layer in the figure */
/* get_min_max_depth() must have already been called */

void
update_layers(void)
{
    Window	 w = XtWindow(layer_canvas);
    int		 i, height;

    /* don't update depth panel when previewing or reading in a Fig file */
    if (preview_in_progress || defer_update_layers)
	return;

    XClearWindow(tool_d, w);
    if (min_depth < 0) return;  /* if no objects, return */

    height = B_BORDER * 3;
    for (i = min_depth; i <= max_depth; i++) {
      if (object_depths[i] > 0) height += TOT_B_HEIGHT;
    }

    /* set height of canvas for all buttons */
    FirstArg(XtNheight, (Dimension)height);
    SetValues(layer_canvas);
    /* force drawing */
    draw_layer_buttons();
}

static void
draw_layer_buttons(void)
{
    Window	 w = XtWindow(layer_canvas);
    int		 i;

    XClearWindow(tool_d, w);
    if (min_depth < 0) return;  /* if no objects, return */

    for (i = min_depth; i <= max_depth; i++) {
        if (object_depths[i] > 0) draw_layer_button(w, i);
    }
}


#define draw_l(w, x1, y1, x2, y2) \
	XDrawLine(tool_d, w, button_gc, x1, y1, x2, y2);

static void
draw_layer_button(Window win, int but)
{
    char	 str[20];
    int		 x, y, w, h, i;

    if (object_depths[but] == 0) return;  /* no object for the depth */

    x = B_BORDER;
    w = B_WIDTH;

    y = B_BORDER;
    for (i = min_depth; i < but; i++) {
      if (object_depths[i]) y += TOT_B_HEIGHT;
    }
    h = TOT_B_HEIGHT;

    /* the whole border */
    draw_l(win,  x,  y,x+w,  y);
    draw_l(win,x+w,  y,x+w,y+h);
    draw_l(win,x+w,y+h,  x,y+h);
    draw_l(win,x,  y+h,  x,  y);

    /* now the checkbox border */
    x=x+B_BORDER; y=y+B_BORDER;
    w=C_SIZE;

    draw_l(win,  x,  y,x+w,  y);
    draw_l(win,x+w,  y,x+w,y+w);
    draw_l(win,x+w,y+w,  x,y+w);
    draw_l(win,  x,y+w,  x,  y);

    /* draw in the checkmark or a blank */
    if (active_layer(but))
	XCopyArea(tool_d, sm_check_pm, win, button_gc, 0, 0,
		sm_check_width, sm_check_height, x+1, y+1);
    else
	XCopyArea(tool_d, sm_null_check_pm, win, button_gc, 0, 0,
		sm_check_width, sm_check_height, x+1, y+1);

    /* now draw in the layer number for the button */
    sprintf(str, "%3d", but);
    XDrawString(tool_d, win, button_gc, x+w+3*B_BORDER, y+w, str, strlen(str));
}

/* this is the callback to refresh the layer buttons that have been exposed */

static void
layer_exposed(Widget w, XExposeEvent *event, String *params, Cardinal *nparams)
{
	(void)params; (void)nparams;
    int		    y, i;

    if (min_depth < 0) return;  /* if no objects return */

    y = 0;
    for (i = min_depth; i <= max_depth; i++) {
      if (object_depths[i] > 0) {
	y += TOT_B_HEIGHT;
	if (event->y <= y && y - TOT_B_HEIGHT <= event->y + event->height)
	  draw_layer_button(XtWindow(w), i);
      }
    }
}

/* come here when user toggles layer button */

static int pressed_but = -1;

static int
calculate_pressed_depth(int y)
{
  int i, y1;

  y1 = TOT_B_HEIGHT;
  for (i = min_depth; i <= max_depth; i++) {
    if (object_depths[i] > 0) {
      if (y < y1) return i;
      y1 += TOT_B_HEIGHT;
    }
  }
  return max_depth + 1;
}

/* user pressed right button on layer checkbox, set depth attribute to that layer */

static void
set_depth_to_layer(Widget w, XButtonEvent *event, String *params, Cardinal *nparams)
{
	(void)w; (void)params; (void)nparams;
    int		    but;

    but = calculate_pressed_depth(event->y);
    if (but < min_depth || max_depth < but)
	return;  /* no such button */
    cur_depth = but;
    show_depth(depth_button);
}

static void
toggle_layer(Widget w, XButtonEvent *event, String *params, Cardinal *nparams)
{
	(void)w; (void)params; (void)nparams;
    Window	    win = XtWindow(layer_canvas);
    int		    but, i;
    Boolean	    obscure;

    if (min_depth < 0) return;  /* if no objects, return */

    but = calculate_pressed_depth(event->y);
    if (but < min_depth || max_depth < but)
	return;  /* no such button */

    /* yes, toggle visibility and redraw */
    active_layers[but] = !active_layers[but];
    draw_layer_button(win, but);
    /* if user just turned on a layer, draw only it if there are no active layers on top */
    if (active_layers[but]) {
	obscure = False;
	for (i=0; i<but; i++)
	    if (active_layers[i]) {
		obscure = True;
		break;
	    }
	if (!obscure) {
	    clearcounts();
	    redisplay_compoundobject(&objects, but);
	} else
	    redisplay_canvas();
    } else {
	/* otherwise redraw whole canvas to get rid of that layer */
	redisplay_canvas();
    }
    pressed_but = but;
}

static void
sweep_layer(Widget w, XButtonEvent *event, String *params, Cardinal *nparams)
{
	(void)w; (void)params; (void)nparams;
    Window	    win = XtWindow(layer_canvas);
    int		    but;
    int		    i;
    Boolean	    changed = FALSE;

    if (min_depth < 0) return;
    if (pressed_but < 0) return;

    but = calculate_pressed_depth(event->y);
    if (but < min_depth) but = min_depth;
    if (max_depth < but) but = max_depth;

    if (pressed_but < but) {
      for (i = pressed_but + 1; i <= but; i++) {
	if (object_depths[i] > 0
	    && active_layers[i] != active_layers[pressed_but]) {
	  active_layers[i] = active_layers[pressed_but];
	  draw_layer_button(win, i);
	  changed = TRUE;
	}
      }
    } else {
      for (i = pressed_but - 1; but <= i; i--) {
	if (object_depths[i] > 0
	    && active_layers[i] != active_layers[pressed_but]) {
	  active_layers[i] = active_layers[pressed_but];
	  draw_layer_button(win, i);
	  changed = TRUE;
	}
      }
    }

    if (changed)
	redisplay_canvas();
}

static void
leave_layer(Widget w, XButtonEvent *event, String *params, Cardinal *nparams)
{
	(void)w; (void)event; (void)params; (void)nparams;
    pressed_but = -1;
}

static void
all_active(Widget w, XtPointer closure, XtPointer call_data)
{
	(void)w; (void)closure; (void)call_data;
    int i;
    Boolean changed = False;

    if (min_depth < 0) return;

    for (i=min_depth; i<=max_depth; i++) {
	if (!active_layers[i]) {
	    active_layers[i] = True;
	    changed = True;
	}
    }
    /* only redisplay if any of the buttons changed */
    if (changed) {
	draw_layer_buttons();
	redisplay_canvas();
    }
}

static void
all_inactive(Widget w, XtPointer closure, XtPointer call_data)
{
	(void)w; (void)closure; (void)call_data;
    int i;
    Boolean changed = False;

    if (min_depth < 0) return;

    for (i=min_depth; i<=max_depth; i++) {
	if (active_layers[i]) {
	    active_layers[i] = False;
	    changed = True;
	}
    }
    /* only redisplay if any of the buttons changed */
    if (changed) {
	draw_layer_buttons();
	redisplay_canvas();
    }
}

static void
toggle_all(Widget w, XtPointer closure, XtPointer call_data)
{
	(void)w; (void)closure; (void)call_data;
    int i;

    if (min_depth < 0) return;

    for (i=min_depth; i<=max_depth; i++) {
	active_layers[i] = !active_layers[i];
    }
    draw_layer_buttons();
    redisplay_canvas();
}

/* when user toggles between gray-out and blank inactive layers */

static void
switch_layer_mode(Widget w, XtPointer closure, XtPointer call_data)
{
	(void)closure; (void)call_data;
    Boolean	    state;
    intptr_t	    which;

    /* check state of the toggle and set/remove checkmark */
    FirstArg(XtNstate, &state);
    GetValues(w);

    if (state ) {
	FirstArg(XtNbitmap, sm_check_pm);
    } else {
	FirstArg(XtNbitmap, sm_null_check_pm);
    }
    SetValues(w);

    /* set the sensitivity of the toggle button to the opposite of its state
       so that the user must press the other one now */
    XtSetSensitive(w, !state);
    /* and make the *other* button the opposite state */
    if (w == graytoggle) {
	XtSetSensitive(blanktoggle, state);
    } else {
	XtSetSensitive(graytoggle, state);
    }
    /* which button */
    which = (intptr_t) XawToggleGetCurrent(w);
    if (which == 0)		/* no buttons on, in transition so return now */
	return;
    if (which == 2)		/* "blank" button, invert state */
	state = !state;

    /* set global state */
    gray_layers = state;

    /* now simply redisplay everything */
    clear_canvas();
    redisplay_canvas();
}

/* return True if *any* object in the compound is in any active layer */

Boolean
any_active_in_compound(F_compound *cmpnd)
{
	F_ellipse  *e;
	F_arc	   *a;
	F_line	   *l;
	F_spline   *s;
	F_text	   *t;
	F_compound *c;

	for (a = cmpnd->arcs; a != NULL; a = a->next)
	    if (active_layer(a->depth))
		return True;
	for (e = cmpnd->ellipses; e != NULL; e = e->next)
	    if (active_layer(e->depth))
		return True;
	for (l = cmpnd->lines; l != NULL; l = l->next)
	    if (active_layer(l->depth))
		return True;
	for (s = cmpnd->splines; s != NULL; s = s->next)
	    if (active_layer(s->depth))
		return True;
	for (t = cmpnd->texts; t != NULL; t = t->next)
	    if (active_layer(t->depth))
		return True;
	for (c = cmpnd->compounds; c != NULL; c = c->next)
	    if (any_active_in_compound(c))
		return True;
	return False;
}

void
update_layerpanel()
{
    /*
     * We must test for the widgets, as this is called by
     * w_cmdpanel.c:refresh_view_menu().
     */

    if (all_active_but) {
	if (appres.showballoons)
	    XAWTIPENABLE(all_active_but, "Display all depths");
	else
	    XawTipDisable(all_active_but);
    }
    if (all_inactive_but) {
	if (appres.showballoons)
	    XAWTIPENABLE(all_inactive_but, "Hide all depths");
	else
	    XawTipDisable(all_inactive_but);
    }
    if (toggle_all_but) {
	if (appres.showballoons)
	    XAWTIPENABLE(toggle_all_but, "Toggle displayed/hidden depths");
	else
	    XawTipDisable(toggle_all_but);
    }
    if (layer_viewp) {
	if (appres.showballoons)
	    XAWTIPENABLE(layer_viewp, "Display or hide any depth");
	else
	    XawTipDisable(layer_viewp);
    }
    if (graylabel) {
	if (appres.showballoons)
	    XAWTIPENABLE(graylabel, "Display inactive layers in gray");
	else
	    XawTipDisable(graylabel);
    }
    if (blanklabel) {
	if (appres.showballoons)
	    XAWTIPENABLE(blanklabel, "Blank inactive layers");
	else
	    XawTipDisable(blanklabel);
    }
}

/* map/unmap the depth manager on the right side */

void
toggle_show_depths(void)
{
    Dimension	wid, b;

    appres.showdepthmanager = !appres.showdepthmanager;
    put_msg("%s depth manager",
		appres.showdepthmanager? "Show": "Don't show");
    /* get actual width of layer form */
    FirstArg(XtNwidth, &wid);
    NextArg(XtNborderWidth, &b);
    GetValues(layer_form);
    if (appres.showdepthmanager) {
	/* make sure it's the right size */
	setup_depth_panel();
	/* make canvas smaller to fit depth manager */
	resize_all(CANVAS_WD-wid-2*b, CANVAS_HT);
    } else {
	XtUnmanageChild(layer_form);
	/* make canvas larger to fill space where depth manager was */
	resize_all(CANVAS_WD+wid+2*b, CANVAS_HT);
    }
    /* update the View menu */
    refresh_view_menu();
    /* and redraw the objects */
    redisplay_canvas();
}

void
save_active_layers(void)
{
    int		 i;
    for (i=0; i<=MAX_DEPTH; i++)
	save_layers[i] = active_layers[i];
}

/* save depth counts and min/max depth */

void
save_depths(void)
{
    int		 i;
    saved_min_depth = min_depth;
    saved_max_depth = max_depth;
    for (i=0; i<=MAX_DEPTH; i++)
	saved_depths[i] = object_depths[i];
}

void
save_counts(void)
{
    int		 i;
    for (i=0; i<=MAX_DEPTH; i++)
	saved_counts[i] = counts[i];
}

void
save_counts_and_clear(void)
{
    save_counts();
    clearallcounts();
}

void
restore_active_layers(void)
{
    int		 i;
    for (i=0; i<=MAX_DEPTH; i++)
	active_layers[i] = save_layers[i];
}

/* restore depth counts and reset min/max depth */

void
restore_depths(void)
{
    int		 i;
    min_depth = saved_min_depth;
    max_depth = saved_max_depth;
    for (i=0; i<=MAX_DEPTH; i++)
	object_depths[i] = saved_depths[i];
    /* refresh the layer manager */
    update_layers();
}

void
restore_counts(void)
{
    int		 i;
    for (i=0; i<=MAX_DEPTH; i++)
	 counts[i] = saved_counts[i];
}

/* swap depth counts and reset min/max depth */

void
swap_depths(void)
{
    int	i;
    int	temp;

    temp = min_depth;
    min_depth = saved_min_depth;
    saved_min_depth = temp;

    temp = max_depth;
    max_depth = saved_max_depth;
    saved_max_depth = temp;

    for (i=0; i<=MAX_DEPTH; i++){
        temp = object_depths[i];
	object_depths[i] = saved_depths[i];
        saved_depths[i] = temp;
    }
    /* refresh the layer manager */
    update_layers();
}

void
swap_counts(void)
{
    int i;
    struct counts temp;
    for (i=0; i<=MAX_DEPTH; i++){
         temp = counts[i];
	 counts[i] = saved_counts[i];
	 saved_counts[i] = temp;
    }
}
