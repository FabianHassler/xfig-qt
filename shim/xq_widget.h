/*
 * xq_widget.h - the C interface between the Xt widget model (xt_widget.c,
 * xt_translate.c) and the Qt user interface.
 */
#ifndef XQ_WIDGET_H
#define XQ_WIDGET_H

#include "xshim.h"

#ifdef __cplusplus
extern "C" {
#endif

extern Display	*xq_display;

/* hooks the Qt UI installs to follow and serve the widgets */
typedef struct {
	void	(*created)(Widget);
	void	(*changed)(Widget, ArgList, Cardinal);
	void	(*managed)(Widget, Boolean);
	void	(*sensitive)(Widget, Boolean);
	void	(*geometry)(Widget);		/* size changed by xfig */
	void	(*popup)(Widget, XtGrabKind);
	void	(*popdown)(Widget);
	void	(*destroyed)(Widget);
	Window	(*window)(Widget);		/* create a window on demand */
	void	(*focus)(Widget);
	void	(*to_global)(Widget, int *x, int *y);
	/* text widgets */
	const char *(*get_string)(Widget);
	void	(*text_replace)(Widget, long from, long to, const char *text);
	long	(*text_insertion)(Widget);
	void	(*set_insertion)(Widget, long);
	/* toggles */
	Boolean	(*get_state)(Widget);
	XtPointer (*toggle_current)(Widget);
	void	(*toggle_set_current)(Widget, XtPointer);
	/* lists and scrollbars */
	void	(*list_change)(Widget, String *, int);
	void	(*list_highlight)(Widget, int);
	int	(*list_current)(Widget);
	void	(*scrollbar_thumb)(Widget, float top, float shown);
	const char *(*clipboard_text)(void);
} XqWidgetHooks;

extern XqWidgetHooks xq_hooks;

/* older, simpler hooks (still called) */
extern void	(*xq_widget_created_hook)(Widget, const char *name, const char *wclass);
extern void	(*xq_widget_setvalues_hook)(Widget, ArgList, Cardinal);

/* the widget record */
const char	*xq_widget_name(Widget w);
const char	*xq_widget_class(Widget w);
const char	*xq_widget_label(Widget w);
Widget	xq_widget_parent(Widget w);
int	xq_widget_num_children(Widget w);
Widget	xq_widget_child(Widget w, int i);
int	xq_widget_width(Widget w);
int	xq_widget_height(Widget w);
int	xq_widget_x(Widget w);
int	xq_widget_y(Widget w);
int	xq_widget_border(Widget w);
Boolean	xq_widget_is_popup(Widget w);
Boolean	xq_widget_popped(Widget w);
Boolean	xq_widget_destroyed(Widget w);
XFontStruct *xq_widget_font(Widget w);
void	*xq_widget_peer(Widget w);
void	xq_widget_set_peer(Widget w, void *peer);
Window	xq_widget_window(Widget w);
void	xq_widget_set_window(Widget w, Window win);
void	xq_widget_set_size(Widget w, int width, int height);
void	xq_widget_set_position(Widget w, int x, int y);

/* resources: the value last set, if any */
Boolean	xq_res(Widget w, const char *name, XtArgVal *value);
XtArgVal xq_res_default(Widget w, const char *name, XtArgVal def);

/* callbacks and events */
int	xq_has_callbacks(Widget w, const char *name);
int	xq_call_callbacks(Widget w, const char *name, XtPointer call_data);
int	xq_widget_event(Widget w, XEvent *ev);	/* handlers + translations */
int	xq_dispatch_event(Widget w, XEvent *ev);	/* translations only */
int	xq_dispatch_accelerators(Widget w, XEvent *ev);
void	xq_forget_accelerators(Widget w);
int	xq_call_action(Widget w, const char *name, XEvent *ev, String *params, Cardinal n);
void	xq_set_translations(Widget w, XtTranslations t);
/* the click count of the event being dispatched (double clicks) */
extern int	xq_click_count;
/* does the widget's translation table handle this event? */
int	xq_translation_matches(Widget w, XEvent *ev);

/* the resource database (command line options, -xrm, Qt settings) */
void	xq_set_resource(const char *spec, const char *value);
const char *xq_get_resource(const char *name);

/* pointer position and keyboard input of the event being dispatched */
void	xq_set_pointer(Window w, int x, int y, unsigned int state);
void	xq_set_key(const char *utf8, KeySym sym);

#ifdef __cplusplus
}
#endif

#endif /* XQ_WIDGET_H */
