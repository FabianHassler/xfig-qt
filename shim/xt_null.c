/*
 * xt_null.c - the rest of the Xt / Xaw / Xlib API used by xfig.
 *
 * Text, list, toggle and scrollbar functions are forwarded to the Qt peers
 * through xq_hooks; window manager, grab and input method calls do nothing.
 * The widget model is in xt_widget.c, timers and the event loop in
 * platform/xt_qt.cpp.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "xshim.h"
#include "xq_widget.h"

/* Xaw */
/* xfig's info balloons: the peer shows XtNtip as its tooltip (ui/xaw.cpp) */
void XawTipEnable(Widget w) { XtVaSetValues(w, "xqTip", (XtArgVal)1, NULL); }
void XawTipDisable(Widget w) { XtVaSetValues(w, "xqTip", (XtArgVal)0, NULL); }

void
XawScrollbarSetThumb(Widget w, float top, float shown)
{
	if (xq_hooks.scrollbar_thumb)
		xq_hooks.scrollbar_thumb(w, top, shown);
}

/* a text widget's "source" is the widget itself */
Widget XawTextGetSource(Widget w) { return w; }

void
XawTextSetInsertionPoint(Widget w, XawTextPosition p)
{
	if (xq_hooks.set_insertion)
		xq_hooks.set_insertion(w, p);
}

XawTextPosition
XawTextGetInsertionPoint(Widget w)
{
	return xq_hooks.text_insertion ? xq_hooks.text_insertion(w) : 0;
}

int
XawTextReplace(Widget w, XawTextPosition from, XawTextPosition to, XawTextBlock *t)
{
	char *text;
	if (!xq_hooks.text_replace)
		return XawEditError;
	text = strndup(t && t->ptr ? t->ptr + t->firstPos : "", t ? t->length : 0);
	xq_hooks.text_replace(w, from, to, text);
	free(text);
	return XawEditDone;
}

/* only scans to the start or the end of all text are used */
XawTextPosition
XawTextSourceScan(Widget w, XawTextPosition p, int type, int dir, int count, Boolean incl)
{
	const char *s;
	(void)type; (void)count; (void)incl;
	if (dir == XawsdLeft)
		return 0;
	s = xq_hooks.get_string ? xq_hooks.get_string(w) : NULL;
	return s ? (XawTextPosition)strlen(s) : p;
}

void XawTextDisplay(Widget w) { (void)w; }
void XawTextSetSelection(Widget w, XawTextPosition a, XawTextPosition b) { (void)w; (void)a; (void)b; }

void
XawFormDoLayout(Widget w, Boolean b)
{
	if (b && xq_hooks.geometry)
		xq_hooks.geometry(w);
}

void
XawListChange(Widget w, String *l, int n, int wd, Boolean r)
{
	(void)wd; (void)r;
	if (n == 0 && l)
		while (l[n])
			++n;
	if (xq_hooks.list_change)
		xq_hooks.list_change(w, l, n);
}

void
XawListHighlight(Widget w, int i)
{
	if (xq_hooks.list_highlight)
		xq_hooks.list_highlight(w, i);
}

void
XawListUnhighlight(Widget w)
{
	if (xq_hooks.list_highlight)
		xq_hooks.list_highlight(w, XAW_LIST_NONE);
}

XawListReturnStruct *
XawListShowCurrent(Widget w)
{
	static XawListReturnStruct cur;
	int i = xq_hooks.list_current ? xq_hooks.list_current(w) : XAW_LIST_NONE;
	XtArgVal list = 0;
	cur.list_index = i;
	cur.string = "";
	if (i >= 0 && xq_res(w, XtNlist, &list) && list)
		cur.string = ((String *)list)[i];
	return &cur;
}

/* toggles in a radio group: the group is identified by its first member */
XtPointer
XawToggleGetCurrent(Widget w)
{
	return xq_hooks.toggle_current ? xq_hooks.toggle_current(w) : NULL;
}

void
XawToggleSetCurrent(Widget w, XtPointer data)
{
	if (xq_hooks.toggle_set_current)
		xq_hooks.toggle_set_current(w, data);
}

void
XawToggleUnsetCurrent(Widget w)
{
	if (xq_hooks.toggle_set_current)
		xq_hooks.toggle_set_current(w, NULL);
}
void XawSimpleMenuAddGlobalActions(XtAppContext a) { (void)a; }
void XawAsciiSourceFreeString(Widget w) { (void)w; }

/* window manager, grabs, input methods: nothing to do */
Atom
XInternAtom(Display *d, const char *n, Bool b)
{
	(void)d; (void)b;
	unsigned long h = 5381;
	while (*n)
		h = h * 33 + (unsigned char)*n++;
	return (Atom)(h | 0x10000);
}
int XUngrabPointer(Display *d, Time t) { (void)d; (void)t; return 1; }
int XGrabPointer(Display *d, Window w, Bool b, unsigned int m, int p, int k, Window c, Cursor u, Time t)
{ (void)d; (void)w; (void)b; (void)m; (void)p; (void)k; (void)c; (void)u; (void)t; return GrabSuccess; }
int XAllowEvents(Display *d, int m, Time t) { (void)d; (void)m; (void)t; return 1; }
Bool
XTranslateCoordinates(Display *d, Window a, Window b, int x, int y, int *rx, int *ry, Window *c)
{
	(void)d; (void)a; (void)b;
	*rx = x; *ry = y;
	if (c) *c = 0;
	return True;
}
Status
XGetWindowAttributes(Display *d, Window w, XWindowAttributes *a)
{
	unsigned int wd = 0, ht = 0, bw, depth;
	Window root;
	int x, y;
	memset(a, 0, sizeof *a);
	XGetGeometry(d, w, &root, &x, &y, &wd, &ht, &bw, &depth);
	a->width = wd;
	a->height = ht;
	a->depth = 24;
	a->visual = xq_default_visual();
	a->map_state = IsViewable;
	a->colormap = 1;
	return 1;
}
int XRecolorCursor(Display *d, Cursor c, XColor *f, XColor *b) { (void)d; (void)c; (void)f; (void)b; return 1; }
int XSynchronize(Display *d, Bool b) { (void)d; (void)b; return 0; }
int XSetWMProtocols(Display *d, Window w, Atom *a, int n) { (void)d; (void)w; (void)a; (void)n; return 1; }
int XSetWMHints(Display *d, Window w, XWMHints *h) { (void)d; (void)w; (void)h; return 1; }
XWMHints *XGetWMHints(Display *d, Window w) { (void)d; (void)w; return calloc(1, sizeof(XWMHints)); }
int XSetIconName(Display *d, Window w, const char *n) { (void)d; (void)w; (void)n; return 1; }
int XSetWindowColormap(Display *d, Window w, Colormap c) { (void)d; (void)w; (void)c; return 1; }
int XSetSubwindowMode(Display *d, GC g, int m) { (void)d; (void)g; (void)m; return 1; }
char *XSetLocaleModifiers(const char *m) { (void)m; return ""; }
Status XSendEvent(Display *d, Window w, Bool b, long m, XEvent *e) { (void)d; (void)w; (void)b; (void)m; (void)e; return 1; }
int XPutBackEvent(Display *d, XEvent *e) { (void)d; (void)e; return 1; }
Bool XFilterEvent(XEvent *e, Window w) { (void)e; (void)w; return False; }
XPixmapFormatValues *
XListPixmapFormats(Display *d, int *n)
{
	XPixmapFormatValues *f = calloc(1, sizeof *f);
	(void)d;
	f->depth = 24;
	f->bits_per_pixel = 32;
	f->scanline_pad = 32;
	*n = 1;
	return f;
}
Colormap *
XListInstalledColormaps(Display *d, Window w, int *n)
{
	Colormap *c = malloc(sizeof *c);
	(void)d; (void)w;
	*c = 1;
	*n = 1;
	return c;
}
Colormap XCreateColormap(Display *d, Window w, Visual *v, int a) { (void)d; (void)w; (void)v; (void)a; return 1; }
Colormap XCopyColormapAndFree(Display *d, Colormap c) { (void)d; return c; }
int XInstallColormap(Display *d, Colormap c) { (void)d; (void)c; return 1; }
int
XGetPointerMapping(Display *d, unsigned char *m, int n)
{
	(void)d;
	for (int i = 0; i < n; ++i)
		m[i] = i + 1;
	return n;
}
int
XGeometry(Display *d, int s, const char *u, const char *def, unsigned bw, unsigned fw,
		unsigned fh, int xa, int ya, int *x, int *y, int *w, int *h)
{
	(void)d; (void)s; (void)u; (void)def; (void)bw; (void)fw; (void)fh; (void)xa; (void)ya;
	*x = *y = 0;
	*w = *h = 0;
	return 0;
}
VisualID XVisualIDFromVisual(Visual *v) { return v ? v->visualid : 0; }
int XResizeWindow(Display *d, Window w, unsigned a, unsigned b) { (void)d; (void)w; (void)a; (void)b; return 1; }
int XMoveWindow(Display *d, Window w, int x, int y) { (void)d; (void)w; (void)x; (void)y; return 1; }
int XMapWindow(Display *d, Window w) { (void)d; (void)w; return 1; }
int XMapRaised(Display *d, Window w) { (void)d; (void)w; return 1; }
int XUnmapWindow(Display *d, Window w) { (void)d; (void)w; return 1; }
Window XCreateSimpleWindow(Display *d, Window p, int x, int y, unsigned w, unsigned h, unsigned b, unsigned long f, unsigned long g)
{ (void)d; (void)p; (void)x; (void)y; (void)w; (void)h; (void)b; (void)f; (void)g; return 0; }
int XStoreName(Display *d, Window w, const char *n) { (void)d; (void)w; (void)n; return 1; }
int XFreeCursor(Display *d, Cursor c) { (void)d; (void)c; return 1; }
Bool XCheckTypedEvent(Display *d, int t, XEvent *e) { (void)d; (void)t; (void)e; return False; }
Bool XCheckMaskEvent(Display *d, long m, XEvent *e) { (void)d; (void)m; (void)e; return False; }
Bool XCheckWindowEvent(Display *d, Window w, long m, XEvent *e) { (void)d; (void)w; (void)m; (void)e; return False; }
int XPending(Display *d) { (void)d; return 0; }

XIM XOpenIM(Display *d, XrmDatabase db, char *a, char *b) { (void)d; (void)db; (void)a; (void)b; return NULL; }
Status XCloseIM(XIM im) { (void)im; return 1; }
char *XGetIMValues(XIM im, ...) { (void)im; return "unsupported"; }
XIC XCreateIC(XIM im, ...) { (void)im; return NULL; }
void XDestroyIC(XIC ic) { (void)ic; }
char *XSetICValues(XIC ic, ...) { (void)ic; return NULL; }
char *XGetICValues(XIC ic, ...) { (void)ic; return "unsupported"; }
void XSetICFocus(XIC ic) { (void)ic; }
void XUnsetICFocus(XIC ic) { (void)ic; }
XVaNestedList XVaCreateNestedList(int u, ...) { (void)u; return NULL; }
int
Xutf8LookupString(XIC ic, XKeyPressedEvent *e, char *buf, int n, KeySym *ks, Status *st)
{
	int len;
	(void)ic;
	len = XLookupString(e, buf, n, ks, NULL);
	if (st)
		*st = len > 0 ? (*ks ? XLookupBoth : XLookupChars) :
			(*ks ? XLookupKeySym : XLookupNone);
	return len;
}
int
Xutf8TextPropertyToTextList(Display *d, const XTextProperty *p, char ***l, int *n)
{
	(void)d; (void)p;
	*l = NULL;
	*n = 0;
	return -1;
}
void XFreeStringList(char **l) { (void)l; }
XFontSet XCreateFontSet(Display *d, const char *b, char ***m, int *n, char **def)
{ (void)d; (void)b; *m = NULL; *n = 0; if (def) *def = ""; return NULL; }
void XFreeFontSet(Display *d, XFontSet f) { (void)d; (void)f; }
