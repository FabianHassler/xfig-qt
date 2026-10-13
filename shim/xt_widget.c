/*
 * xt_widget.c - the widget model behind the Xt / Xaw API (see xtshim.h).
 *
 * A widget is a record holding its name, class, parent, children, resources,
 * callbacks and event handlers.  The Qt user interface installs hooks
 * (struct XqWidgetHooks) and builds a Qt "peer" for each widget it wants to
 * show; it reads the resources stored here and calls back into xfig through
 * xq_call_callbacks() and xq_widget_event().
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "xshim.h"
#include "xq_widget.h"

/* ------------------------------------------------------------- records */

typedef struct Resource {
	char		*name;
	XtArgVal	value;
	struct Resource	*next;
} Resource;

typedef struct Callback {
	char		*name;
	XtCallbackProc	proc;
	XtPointer	closure;
	struct Callback	*next;
} Callback;

typedef struct Handler {
	EventMask	mask;
	XtEventHandler	proc;
	XtPointer	closure;
	struct Handler	*next;
} Handler;

struct _WidgetRec {
	char		*name;
	Widget		parent;
	WidgetClass	wclass;
	Window		window;
	Boolean		sensitive;
	Boolean		managed;
	Boolean		popped;
	Boolean		is_popup;	/* a popup shell child */
	Boolean		destroyed;
	Dimension	width, height, border;
	Position	x, y;
	XFontStruct	*font;
	Resource	*res;
	Callback	*callbacks;
	Handler		*handlers;
	Widget		*children;
	int		nchildren;
	void		*peer;
	char		*string;	/* XtNstring / XtNlabel copies handed out */
};

struct _WidgetClassRec { const char *name; };

#define WCLASS(var, n) \
	static struct _WidgetClassRec var##_rec = { n }; \
	WidgetClass var = &var##_rec;

WCLASS(applicationShellWidgetClass, "ApplicationShell")
WCLASS(transientShellWidgetClass, "TransientShell")
WCLASS(overrideShellWidgetClass, "OverrideShell")
WCLASS(topLevelShellWidgetClass, "TopLevelShell")
WCLASS(labelWidgetClass, "Label")
WCLASS(commandWidgetClass, "Command")
WCLASS(formWidgetClass, "Form")
WCLASS(menuButtonWidgetClass, "MenuButton")
WCLASS(asciiTextWidgetClass, "AsciiText")
WCLASS(toggleWidgetClass, "Toggle")
WCLASS(viewportWidgetClass, "Viewport")
WCLASS(smeBSBObjectClass, "SmeBSB")
WCLASS(smeObjectClass, "Sme")
WCLASS(smeLineObjectClass, "SmeLine")
WCLASS(scrollbarWidgetClass, "Scrollbar")
WCLASS(boxWidgetClass, "Box")
WCLASS(simpleMenuWidgetClass, "SimpleMenu")
WCLASS(listWidgetClass, "List")
WCLASS(panedWidgetClass, "Paned")
WCLASS(dialogWidgetClass, "Dialog")
WCLASS(simpleWidgetClass, "Simple")
WCLASS(coreWidgetClass, "Core")
WCLASS(widgetClass, "Widget")
WCLASS(figListWidgetClass, "FigList")

XqWidgetHooks	xq_hooks;
void	(*xq_widget_created_hook)(Widget, const char *name, const char *wclass) = NULL;
void	(*xq_widget_setvalues_hook)(Widget, ArgList, Cardinal) = NULL;

/* the resources whose (string) values must be copied */
static Boolean
is_string_resource(const char *n)
{
	return !strcmp(n, XtNlabel) || !strcmp(n, XtNstring) ||
		!strcmp(n, XtNtitle) || !strcmp(n, XtNmenuName) ||
		!strcmp(n, XtNtip);
}

static XFontStruct *
default_font(void)
{
	static XFontStruct *f;
	if (!f)
		f = XLoadQueryFont(xq_display, "6x13");
	return f;
}

static void
set_res(Widget w, const char *name, XtArgVal value)
{
	Resource *r;
	if (is_string_resource(name) && value)
		value = (XtArgVal)strdup((const char *)value);
	for (r = w->res; r; r = r->next)
		if (!strcmp(r->name, name)) {
			if (is_string_resource(name) && r->value)
				free((char *)r->value);
			r->value = value;
			return;
		}
	r = malloc(sizeof *r);
	r->name = strdup(name);
	r->value = value;
	r->next = w->res;
	w->res = r;
}

Boolean
xq_res(Widget w, const char *name, XtArgVal *value)
{
	if (!w)
		return False;
	for (Resource *r = w->res; r; r = r->next)
		if (!strcmp(r->name, name)) {
			if (value)
				*value = r->value;
			return True;
		}
	return False;
}

XtArgVal
xq_res_default(Widget w, const char *name, XtArgVal def)
{
	XtArgVal v;
	return xq_res(w, name, &v) ? v : def;
}

static void add_callback_list(Widget w, const char *name, XtCallbackList l);

static void
store_args(Widget w, ArgList a, Cardinal n)
{
	for (Cardinal i = 0; i < n; ++i) {
		const char *nm = a[i].name;
		XtArgVal v = a[i].value;
		if (!nm)
			continue;
		if (!strcmp(nm, XtNwidth))
			w->width = (Dimension)v;
		else if (!strcmp(nm, XtNheight))
			w->height = (Dimension)v;
		else if (!strcmp(nm, XtNborderWidth))
			w->border = (Dimension)v;
		else if (!strcmp(nm, XtNx))
			w->x = (Position)v;
		else if (!strcmp(nm, XtNy))
			w->y = (Position)v;
		else if (!strcmp(nm, XtNsensitive))
			w->sensitive = (Boolean)v;
		else if (!strcmp(nm, XtNfont))
			w->font = (XFontStruct *)v;
		else if (!strcmp(nm, XtNtranslations))
			xq_set_translations(w, (XtTranslations)v);
		else if (!strcmp(nm, XtNcallback) || !strcmp(nm, XtNjumpProc) ||
				!strcmp(nm, XtNscrollProc) ||
				!strcmp(nm, XtNdestroyCallback))
			add_callback_list(w, nm, (XtCallbackList)v);
		else {
			set_res(w, nm, v);
			if (!strcmp(nm, XtNbackgroundPixmap) && w->window) {
				/* X shows the new background at once */
				XSetWindowBackgroundPixmap(xq_display, w->window, (Pixmap)v);
				XClearWindow(xq_display, w->window);
			} else if (!strcmp(nm, XtNbackground) && w->window) {
				XSetWindowBackground(xq_display, w->window, (unsigned long)v);
			}
		}
	}
}

/* answer a resource query */
static void
get_arg(Widget w, const char *nm, XtArgVal dst)
{
	XtArgVal v;
	if (!w || !nm || !dst)
		return;
	if (!strcmp(nm, XtNwidth))
		*(Dimension *)dst = w->width;
	else if (!strcmp(nm, XtNheight))
		*(Dimension *)dst = w->height;
	else if (!strcmp(nm, XtNborderWidth))
		*(Dimension *)dst = w->border;
	else if (!strcmp(nm, XtNx))
		*(Position *)dst = w->x;
	else if (!strcmp(nm, XtNy))
		*(Position *)dst = w->y;
	else if (!strcmp(nm, XtNsensitive))
		*(Boolean *)dst = w->sensitive;
	else if (!strcmp(nm, XtNfont))
		*(XFontStruct **)dst = w->font ? w->font : default_font();
	else if (!strcmp(nm, XtNtextSource))
		*(Widget *)dst = w;	/* a text widget is its own source */
	else if (!strcmp(nm, XtNstring) && xq_hooks.get_string &&
			xq_hooks.get_string(w)) {
		/* the text the user typed lives in the Qt peer */
		free(w->string);
		w->string = strdup(xq_hooks.get_string(w));
		*(char **)dst = w->string;
	} else if (!strcmp(nm, XtNstate) && xq_hooks.get_state && w->peer)
		*(Boolean *)dst = xq_hooks.get_state(w);
	else if (xq_res(w, nm, &v)) {
		if (!strcmp(nm, XtNinternalWidth) || !strcmp(nm, XtNinternalHeight) ||
				!strcmp(nm, XtNhorizDistance) ||
				!strcmp(nm, XtNvertDistance) ||
				!strcmp(nm, XtNdefaultDistance))
			*(Dimension *)dst = (Dimension)v;
		else if (!strcmp(nm, XtNstate) || !strcmp(nm, XtNresizable) ||
				!strcmp(nm, XtNinternational))
			*(Boolean *)dst = (Boolean)v;
		else if (!strcmp(nm, XtNjustify) || !strcmp(nm, XtNorientation))
			*(int *)dst = (int)v;
		else if (!strcmp(nm, XtNthumb) || !strcmp(nm, "shown") ||
				!strcmp(nm, "topOfThumb"))
			*(float *)dst = *(float *)&v;
		else
			*(XtArgVal *)dst = v;	/* pointers, pixels, pixmaps */
	} else if (!strcmp(nm, XtNbackground))
		*(Pixel *)dst = 0xffffff;
	else if (!strcmp(nm, XtNforeground) || !strcmp(nm, XtNborderColor))
		*(Pixel *)dst = 0x000000;
	else if (!strcmp(nm, XtNstate))
		*(Boolean *)dst = False;
	else if (!strcmp(nm, XtNlabel) || !strcmp(nm, XtNstring))
		*(char **)dst = (char *)(w->name ? w->name : "");
	else if (!strcmp(nm, XtNinternalWidth) || !strcmp(nm, XtNinternalHeight))
		*(Dimension *)dst = 4;
	else if (!strcmp(nm, XtNhorizDistance) || !strcmp(nm, XtNvertDistance) ||
			!strcmp(nm, XtNdefaultDistance))
		*(Dimension *)dst = 4;
	else if (!strcmp(nm, XtNbitmap) || !strcmp(nm, XtNleftBitmap) ||
			!strcmp(nm, XtNbackgroundPixmap))
		*(Pixmap *)dst = None;
	else if (!strcmp(nm, XtNinsertPosition))
		*(XawTextPosition *)dst = xq_hooks.text_insertion ?
			xq_hooks.text_insertion(w) : 0;
}

/* ----------------------------------------------------------- creation */

static void
add_child(Widget parent, Widget w)
{
	if (!parent)
		return;
	parent->children = realloc(parent->children,
			(parent->nchildren + 1) * sizeof(Widget));
	parent->children[parent->nchildren++] = w;
}

static Widget
create(const char *name, WidgetClass wc, Widget parent, ArgList a, Cardinal n,
		Boolean managed, Boolean popup)
{
	Widget w = calloc(1, sizeof(struct _WidgetRec));
	w->name = strdup(name ? name : "");
	w->parent = parent;
	w->wclass = wc;
	w->sensitive = True;
	w->managed = managed;
	w->is_popup = popup;
	store_args(w, a, n);
	add_child(parent, w);
	if (xq_hooks.created)
		xq_hooks.created(w);
	/* backwards compatibility with the first, simpler hook */
	if (xq_widget_created_hook)
		xq_widget_created_hook(w, w->name, wc ? wc->name : "");
	return w;
}

static Widget
create_va(const char *name, WidgetClass wc, Widget parent, Boolean managed,
		Boolean popup, va_list ap)
{
	Arg args[64];
	Cardinal n = 0;
	const char *nm;
	while (n < 64 && (nm = va_arg(ap, const char *)) != NULL) {
		args[n].name = (String)nm;
		args[n].value = va_arg(ap, XtArgVal);
		++n;
	}
	return create(name, wc, parent, args, n, managed, popup);
}

Widget XtCreateManagedWidget(const char *n, WidgetClass c, Widget p, ArgList a, Cardinal k)
{ return create(n, c, p, a, k, True, False); }
Widget XtCreateWidget(const char *n, WidgetClass c, Widget p, ArgList a, Cardinal k)
{ return create(n, c, p, a, k, False, False); }
Widget XtCreatePopupShell(const char *n, WidgetClass c, Widget p, ArgList a, Cardinal k)
{ return create(n, c, p, a, k, False, True); }
Widget XtVaCreateManagedWidget(const char *n, WidgetClass c, Widget p, ...)
{ va_list ap; va_start(ap, p); Widget w = create_va(n, c, p, True, False, ap); va_end(ap); return w; }
Widget XtVaCreateWidget(const char *n, WidgetClass c, Widget p, ...)
{ va_list ap; va_start(ap, p); Widget w = create_va(n, c, p, False, False, ap); va_end(ap); return w; }
Widget XtVaCreatePopupShell(const char *n, WidgetClass c, Widget p, ...)
{ va_list ap; va_start(ap, p); Widget w = create_va(n, c, p, False, True, ap); va_end(ap); return w; }
Widget XtAppCreateShell(const char *n, const char *cl, WidgetClass c, Display *d, ArgList a, Cardinal k)
{ (void)cl; (void)d; return create(n, c, NULL, a, k, True, False); }

/* --------------------------------------------------------- accessors */

const char *xq_widget_name(Widget w) { return w ? w->name : ""; }
const char *xq_widget_class(Widget w) { return w && w->wclass ? w->wclass->name : ""; }
const char *
xq_widget_label(Widget w)
{
	XtArgVal v;
	if (xq_res(w, XtNlabel, &v) && v)
		return (const char *)v;
	return w ? w->name : "";
}
Widget xq_widget_parent(Widget w) { return w ? w->parent : NULL; }
int xq_widget_num_children(Widget w) { return w ? w->nchildren : 0; }
Widget xq_widget_child(Widget w, int i) { return w && i >= 0 && i < w->nchildren ? w->children[i] : NULL; }
int xq_widget_width(Widget w) { return w ? w->width : 0; }
int xq_widget_height(Widget w) { return w ? w->height : 0; }
int xq_widget_x(Widget w) { return w ? w->x : 0; }
int xq_widget_y(Widget w) { return w ? w->y : 0; }
int xq_widget_border(Widget w) { return w ? w->border : 0; }
Boolean xq_widget_is_popup(Widget w) { return w ? w->is_popup : False; }
Boolean xq_widget_popped(Widget w) { return w ? w->popped : False; }
Boolean xq_widget_destroyed(Widget w) { return w ? w->destroyed : True; }
XFontStruct *xq_widget_font(Widget w) { return w && w->font ? w->font : default_font(); }
void *xq_widget_peer(Widget w) { return w ? w->peer : NULL; }
void xq_widget_set_peer(Widget w, void *p) { if (w) w->peer = p; }
void xq_widget_set_window(Widget w, Window win) { if (w) w->window = win; }
Window xq_widget_window(Widget w) { return w ? w->window : 0; }
void
xq_widget_set_size(Widget w, int width, int height)
{
	if (w) {
		w->width = (Dimension)width;
		w->height = (Dimension)height;
	}
}
void
xq_widget_set_position(Widget w, int x, int y)
{
	if (w) {
		w->x = (Position)x;
		w->y = (Position)y;
	}
}

/* ------------------------------------------------------- Xt functions */

void
XtSetValues(Widget w, ArgList a, Cardinal n)
{
	if (!w)
		return;
	store_args(w, a, n);
	if (xq_hooks.changed)
		xq_hooks.changed(w, a, n);
	if (xq_widget_setvalues_hook)
		xq_widget_setvalues_hook(w, a, n);
}

void
XtGetValues(Widget w, ArgList a, Cardinal n)
{
	for (Cardinal i = 0; i < n; ++i)
		get_arg(w, a[i].name, a[i].value);
}

void
XtVaSetValues(Widget w, ...)
{
	Arg args[64];
	Cardinal n = 0;
	const char *nm;
	va_list ap;
	va_start(ap, w);
	while (n < 64 && (nm = va_arg(ap, const char *)) != NULL) {
		args[n].name = (String)nm;
		args[n].value = va_arg(ap, XtArgVal);
		++n;
	}
	va_end(ap);
	XtSetValues(w, args, n);
}

void
XtVaGetValues(Widget w, ...)
{
	const char *nm;
	va_list ap;
	va_start(ap, w);
	while ((nm = va_arg(ap, const char *)) != NULL)
		get_arg(w, nm, va_arg(ap, XtArgVal));
	va_end(ap);
}

void
XtSetSensitive(Widget w, Boolean s)
{
	if (!w)
		return;
	w->sensitive = s;
	if (xq_hooks.sensitive)
		xq_hooks.sensitive(w, s);
}

Boolean
XtIsSensitive(Widget w)
{
	/* insensitive if any ancestor is */
	for (; w; w = w->parent)
		if (!w->sensitive)
			return False;
	return True;
}

Boolean XtIsRealized(Widget w) { return w != NULL && !w->destroyed; }
Boolean XtIsManaged(Widget w) { return w ? w->managed : False; }

Window
XtWindow(Widget w)
{
	if (!w || w->destroyed)
		return 0;
	if (!w->window && xq_hooks.window)
		w->window = xq_hooks.window(w);
	return w->window;
}

Display *XtDisplay(Widget w) { (void)w; return xq_display; }
Screen *XtScreen(Widget w) { (void)w; return (Screen *)xq_display; }
String XtName(Widget w) { return w ? w->name : ""; }
Widget XtParent(Widget w) { return w ? w->parent : NULL; }

static void
set_managed(Widget w, Boolean m)
{
	if (!w || w->managed == m)
		return;
	w->managed = m;
	if (xq_hooks.managed)
		xq_hooks.managed(w, m);
}

void XtManageChild(Widget w) { set_managed(w, True); }
void XtUnmanageChild(Widget w) { set_managed(w, False); }
void XtManageChildren(WidgetList l, Cardinal n) { for (Cardinal i = 0; i < n; ++i) set_managed(l[i], True); }
void XtUnmanageChildren(WidgetList l, Cardinal n) { for (Cardinal i = 0; i < n; ++i) set_managed(l[i], False); }

static void
remove_child(Widget parent, Widget w)
{
	if (!parent)
		return;
	for (int i = 0; i < parent->nchildren; ++i)
		if (parent->children[i] == w) {
			memmove(&parent->children[i], &parent->children[i + 1],
				(parent->nchildren - i - 1) * sizeof(Widget));
			--parent->nchildren;
			return;
		}
}

static void
destroy(Widget w)
{
	/* children first; the records stay allocated, as callers may still
	   hold them, but are marked */
	while (w->nchildren > 0) {
		Widget c = w->children[w->nchildren - 1];
		--w->nchildren;
		c->parent = NULL;
		destroy(c);
	}
	xq_call_callbacks(w, XtNdestroyCallback, NULL);
	w->destroyed = True;
	if (xq_hooks.destroyed)
		xq_hooks.destroyed(w);
	w->peer = NULL;
}

void
XtDestroyWidget(Widget w)
{
	if (!w || w->destroyed)
		return;
	remove_child(w->parent, w);
	destroy(w);
}

void
XtPopup(Widget w, XtGrabKind k)
{
	if (!w)
		return;
	w->popped = True;
	if (xq_hooks.popup)
		xq_hooks.popup(w, k);
}

void XtPopupSpringLoaded(Widget w) { XtPopup(w, XtGrabExclusive); }

void
XtPopdown(Widget w)
{
	if (!w || !w->popped)
		return;
	w->popped = False;
	if (xq_hooks.popdown)
		xq_hooks.popdown(w);
}

void XtRealizeWidget(Widget w) { (void)w; }

void
XtResizeWidget(Widget w, Dimension wd, Dimension ht, Dimension b)
{
	if (!w)
		return;
	w->width = wd;
	w->height = ht;
	w->border = b;
	if (xq_hooks.geometry)
		xq_hooks.geometry(w);
}

void XtMapWidget(Widget w) { set_managed(w, True); }
void XtUnmapWidget(Widget w) { set_managed(w, False); }

/* find a widget by name below w; "*name" searches the whole subtree */
static Widget
find_name(Widget w, const char *name, Boolean deep)
{
	for (int i = 0; i < w->nchildren; ++i)
		if (!strcmp(w->children[i]->name, name))
			return w->children[i];
	if (deep)
		for (int i = 0; i < w->nchildren; ++i) {
			Widget f = find_name(w->children[i], name, True);
			if (f)
				return f;
		}
	return NULL;
}

Widget
XtNameToWidget(Widget w, const char *names)
{
	char buf[256];
	const char *s = names;
	while (w && *s) {
		Boolean deep = False;
		int n = 0;
		if (*s == '*') { deep = True; ++s; }
		else if (*s == '.') { ++s; }
		while (*s && *s != '.' && *s != '*' && n < 255)
			buf[n++] = *s++;
		buf[n] = '\0';
		if (n == 0)
			break;
		w = find_name(w, buf, deep);
	}
	return w;
}

/* -------------------------------------------------------- callbacks */

static void
add_callback(Widget w, const char *name, XtCallbackProc proc, XtPointer closure)
{
	Callback *c = malloc(sizeof *c), **tail;
	c->name = strdup(name);
	c->proc = proc;
	c->closure = closure;
	c->next = NULL;
	for (tail = &w->callbacks; *tail; tail = &(*tail)->next)
		;
	*tail = c;
}

static void
add_callback_list(Widget w, const char *name, XtCallbackList l)
{
	for (; l && l->callback; ++l)
		add_callback(w, name, l->callback, l->closure);
}

void
XtAddCallback(Widget w, String name, XtCallbackProc proc, XtPointer closure)
{
	if (w && proc)
		add_callback(w, name, proc, closure);
}

void
XtRemoveAllCallbacks(Widget w, const char *name)
{
	Callback **c = &w->callbacks;
	while (*c) {
		if (!strcmp((*c)->name, name)) {
			Callback *d = *c;
			*c = d->next;
			free(d->name);
			free(d);
		} else {
			c = &(*c)->next;
		}
	}
}

int
xq_has_callbacks(Widget w, const char *name)
{
	for (Callback *c = w ? w->callbacks : NULL; c; c = c->next)
		if (!strcmp(c->name, name))
			return 1;
	return 0;
}

int
xq_call_callbacks(Widget w, const char *name, XtPointer call_data)
{
	int n = 0;
	if (!w)
		return 0;
	/* a callback may destroy the widget or change the list: copy first */
	XtCallbackProc procs[32];
	XtPointer closures[32];
	for (Callback *c = w->callbacks; c && n < 32; c = c->next)
		if (!strcmp(c->name, name)) {
			procs[n] = c->proc;
			closures[n] = c->closure;
			++n;
		}
	for (int i = 0; i < n; ++i)
		procs[i](w, closures[i], call_data);
	return n;
}

void XtCallCallbacks(Widget w, const char *n, XtPointer d) { xq_call_callbacks(w, n, d); }

/* ----------------------------------------------------- event handlers */

void
XtAddEventHandler(Widget w, EventMask m, Boolean nm, XtEventHandler proc, XtPointer cl)
{
	(void)nm;
	if (!w || !proc)
		return;
	Handler *h = malloc(sizeof *h);
	h->mask = m;
	h->proc = proc;
	h->closure = cl;
	h->next = w->handlers;
	w->handlers = h;
}

void
XtRemoveEventHandler(Widget w, EventMask m, Boolean nm, XtEventHandler proc, XtPointer cl)
{
	(void)nm;
	for (Handler **h = &w->handlers; *h; h = &(*h)->next)
		if ((*h)->proc == proc && (*h)->closure == cl) {
			Handler *d = *h;
			d->mask &= ~m;
			if (!d->mask) {
				*h = d->next;
				free(d);
			}
			return;
		}
}

static EventMask
event_mask(XEvent *ev)
{
	switch (ev->type) {
	case KeyPress:		return KeyPressMask;
	case KeyRelease:	return KeyReleaseMask;
	case ButtonPress:	return ButtonPressMask;
	case ButtonRelease:	return ButtonReleaseMask;
	case MotionNotify: {
		EventMask m = PointerMotionMask;
		unsigned s = ev->xmotion.state;
		if (s & (Button1Mask | Button2Mask | Button3Mask))
			m |= ButtonMotionMask;
		if (s & Button1Mask) m |= Button1MotionMask;
		if (s & Button2Mask) m |= Button2MotionMask;
		if (s & Button3Mask) m |= Button3MotionMask;
		return m;
	}
	case EnterNotify:	return EnterWindowMask;
	case LeaveNotify:	return LeaveWindowMask;
	case Expose:		return ExposureMask;
	case ConfigureNotify:	return StructureNotifyMask;
	default:		return 0;
	}
}

/* run the event handlers, then the translations; 1 if anything ran */
int
xq_widget_event(Widget w, XEvent *ev)
{
	int handled = 0;
	EventMask m = event_mask(ev);
	if (!w || w->destroyed)
		return 0;
	/* handlers may remove themselves: collect first */
	XtEventHandler procs[16];
	XtPointer closures[16];
	int n = 0;
	for (Handler *h = w->handlers; h && n < 16; h = h->next)
		if (h->mask & m) {
			procs[n] = h->proc;
			closures[n] = h->closure;
			++n;
		}
	for (int i = 0; i < n; ++i) {
		Boolean cont = True;
		procs[i](w, closures[i], ev, &cont);
		handled = 1;
	}
	if (!w->destroyed && xq_dispatch_event(w, ev))
		handled = 1;
	return handled;
}

void
XtTranslateCoords(Widget w, Position x, Position y, Position *rx, Position *ry)
{
	int ax = x, ay = y;
	if (xq_hooks.to_global)
		xq_hooks.to_global(w, &ax, &ay);
	*rx = (Position)ax;
	*ry = (Position)ay;
}

/* ------------------------------------------------ resource database */

typedef struct DbEntry {
	char	*name;
	char	*value;
	struct DbEntry *next;
} DbEntry;

static DbEntry *db = NULL;

/* "Fig.splash", "*splash", ".splash" all name the resource "splash" */
static const char *
db_name(const char *spec)
{
	const char *p = strrchr(spec, '.');
	const char *q = strrchr(spec, '*');
	if (q > p)
		p = q;
	return p ? p + 1 : spec;
}

void
xq_set_resource(const char *spec, const char *value)
{
	const char *n = db_name(spec);
	for (DbEntry *r = db; r; r = r->next)
		if (!strcmp(r->name, n)) {
			free(r->value);
			r->value = strdup(value);
			return;
		}
	DbEntry *r = malloc(sizeof *r);
	r->name = strdup(n);
	r->value = strdup(value);
	r->next = db;
	db = r;
}

const char *
xq_get_resource(const char *name)
{
	for (DbEntry *r = db; r; r = r->next)
		if (!strcmp(r->name, name))
			return r->value;
	return NULL;
}

/* parse the command line with the option table, as XtAppInitialize does */
static void
parse_options(XrmOptionDescList opts, Cardinal nopts, int *argc, String *argv)
{
	int i = 1, out = 1;
	while (i < *argc) {
		const char *arg = argv[i];
		XrmOptionDescRec *match = NULL;
		int nmatch = 0;
		for (Cardinal k = 0; k < nopts; ++k) {
			if (!strcmp(opts[k].option, arg)) {
				match = &opts[k];
				nmatch = 1;
				break;
			}
			/* unique abbreviations are allowed */
			if (arg[0] == '-' && strlen(arg) > 1 &&
					!strncmp(opts[k].option, arg, strlen(arg))) {
				match = &opts[k];
				++nmatch;
			}
		}
		if (nmatch != 1) {
			argv[out++] = argv[i++];
			continue;
		}
		switch (match->argKind) {
		case XrmoptionNoArg:
			xq_set_resource(match->specifier, (char *)match->value);
			i += 1;
			break;
		case XrmoptionSepArg:
			if (i + 1 < *argc)
				xq_set_resource(match->specifier, argv[i + 1]);
			i += 2;
			break;
		case XrmoptionResArg:
			if (i + 1 < *argc) {
				/* -xrm "Fig.resource: value" */
				char *spec = strdup(argv[i + 1]);
				char *colon = strchr(spec, ':');
				if (colon) {
					const char *v = colon + 1;
					*colon = '\0';
					while (*v == ' ' || *v == '\t')
						++v;
					xq_set_resource(spec, v);
				}
				free(spec);
			}
			i += 2;
			break;
		case XrmoptionStickyArg:
			xq_set_resource(match->specifier, arg + strlen(match->option));
			i += 1;
			break;
		case XrmoptionIsArg:
			xq_set_resource(match->specifier, arg);
			i += 1;
			break;
		case XrmoptionSkipArg:
			argv[out++] = argv[i++];
			if (i < *argc)
				argv[out++] = argv[i++];
			break;
		default:
			argv[out++] = argv[i++];
			break;
		}
	}
	argv[out] = NULL;
	*argc = out;
}

Widget
XtAppInitialize(XtAppContext *app, const char *cl, XrmOptionDescList o, Cardinal no,
		int *argc, String *argv, String *fb, ArgList a, Cardinal k)
{
	(void)cl; (void)fb;
	if (argc && argv)
		parse_options(o, no, argc, argv);
	if (app)
		*app = (XtAppContext)1;
	return create("xfig_init", applicationShellWidgetClass, NULL, a, k, True, False);
}

/* fill in application resources: database values, then the defaults */
void
XtGetApplicationResources(Widget w, XtPointer base, XtResourceList res, Cardinal n,
		ArgList a, Cardinal na)
{
	(void)w; (void)a; (void)na;
	for (Cardinal i = 0; i < n; ++i) {
		char *dst = (char *)base + res[i].resource_offset;
		const char *dtype = res[i].default_type;
		XtPointer def = res[i].default_addr;
		const char *set = xq_get_resource(res[i].resource_name);
		if (set) {
			dtype = XtRString;
			def = (XtPointer)set;
			if (!strcmp(res[i].resource_type, XtRString)) {
				*(char **)dst = strdup(set);
				continue;
			}
		}
		if (!strcmp(dtype, XtRImmediate)) {
			/* the value itself is stored in the pointer */
			if (!strcmp(res[i].resource_type, XtRString))
				*(char **)dst = (char *)def;
			else if (res[i].resource_size == sizeof(int))
				*(int *)dst = (int)(intptr_t)def;
			else if (res[i].resource_size == sizeof(char))
				*(char *)dst = (char)(intptr_t)def;
			else
				memcpy(dst, &def, res[i].resource_size);
		} else if (!strcmp(dtype, XtRString) &&
				!strcmp(res[i].resource_type, XtRString)) {
			*(char **)dst = (char *)def;
		} else if (!strcmp(dtype, XtRString)) {
			/* a string to convert */
			const char *s = (const char *)def;
			if (!s)
				continue;
			if (!strcmp(res[i].resource_type, XtRFloat))
				*(float *)dst = (float)atof(s);
			else if (!strcmp(res[i].resource_type, XtRInt))
				*(int *)dst = atoi(s);
			else if (!strcmp(res[i].resource_type, XtRBoolean))
				*(Boolean *)dst = (!strcasecmp(s, "true") ||
						!strcasecmp(s, "on") ||
						!strcasecmp(s, "yes"));
		} else if (def) {
			memcpy(dst, def, res[i].resource_size);
		}
	}
}

/* ------------------------------------------------------- misc Xt */

char *XtMalloc(Cardinal n) { return malloc(n ? n : 1); }
char *XtCalloc(Cardinal n, Cardinal s) { return calloc(n ? n : 1, s ? s : 1); }
char *XtRealloc(char *p, Cardinal n) { return realloc(p, n ? n : 1); }
void XtFree(char *p) { free(p); }
String XtNewString(const char *s) { return s ? strdup(s) : NULL; }

void
XtSetKeyboardFocus(Widget tree, Widget w)
{
	(void)tree;
	if (w && xq_hooks.focus)
		xq_hooks.focus(w);
}

XtAppContext XtWidgetToApplicationContext(Widget w) { (void)w; return (XtAppContext)1; }
void XtGetActionList(WidgetClass c, XtActionList *l, Cardinal *n) { (void)c; *l = NULL; *n = 0; }
void XtInstallAccelerators(Widget a, Widget b) { (void)a; (void)b; }
void XtInstallAllAccelerators(Widget a, Widget b) { (void)a; (void)b; }
Boolean XtDispatchEvent(XEvent *e) { (void)e; return False; }
void XtWarning(const char *m) { fprintf(stderr, "xfig: %s\n", m); }
void
XtGetSelectionValue(Widget w, Atom sel, Atom target, XtSelectionCallbackProc cb,
		XtPointer closure, Time t)
{
	(void)sel; (void)t;
	/* deliver the clipboard text, as the selection owner would */
	const char *text = xq_hooks.clipboard_text ? xq_hooks.clipboard_text() : NULL;
	unsigned long len = text ? strlen(text) : 0;
	int format = 8;
	Atom type = XA_STRING;
	char *copy = text ? strdup(text) : NULL;
	cb(w, closure, &sel, &type, copy, &len, &format);
	(void)target;
}
void XtStringConversionWarning(const char *a, const char *b) { (void)a; (void)b; }
XtLanguageProc XtSetLanguageProc(XtAppContext a, XtLanguageProc p, XtPointer c) { (void)a; (void)p; (void)c; return NULL; }
int XtMakeResizeRequest(Widget w, Dimension wd, Dimension ht, Dimension *rw, Dimension *rh)
{
	XtResizeWidget(w, wd, ht, w ? w->border : 0);
	if (rw) *rw = wd;
	if (rh) *rh = ht;
	return 0;	/* XtGeometryYes */
}
int XtMakeGeometryRequest(Widget w, XtWidgetGeometry *req, XtWidgetGeometry *reply)
{
	(void)reply;
	if (w && req)
		XtResizeWidget(w, req->width ? req->width : w->width,
				req->height ? req->height : w->height, w->border);
	return 0;
}
void XtAppAddConverter(XtAppContext a, const char *f, const char *t, XtConverter c, XtConvertArgList l, Cardinal n)
{ (void)a; (void)f; (void)t; (void)c; (void)l; (void)n; }
void XtAddGrab(Widget w, Boolean a, Boolean b) { (void)w; (void)a; (void)b; }
void XtRemoveGrab(Widget w) { (void)w; }
void XtAppSetErrorHandler(XtAppContext a, void (*h)(String)) { (void)a; (void)h; }
void XtAppSetWarningHandler(XtAppContext a, void (*h)(String)) { (void)a; (void)h; }
