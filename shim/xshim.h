/*
 * xshim.h - a minimal stand-in for the X11 / Xt / Xft headers.
 *
 * xfig's core was written against Xlib.  Instead of rewriting every drawing
 * call, this header declares the small subset of Xlib types, constants and
 * functions that the core uses.  The functions are implemented on top of Qt
 * (QImage/QPainter, QFont) in platform/xlib_qt.cpp.  Xt/Xaw widget types are
 * only declared so that headers parse; the widget code itself is replaced by
 * the Qt user interface in ui/.
 */
#ifndef XSHIM_H
#define XSHIM_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fontconfig/fontconfig.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ X.h */

typedef unsigned long	XID;
typedef unsigned long	Atom;
typedef unsigned long	VisualID;
typedef unsigned long	Time;
typedef XID		Window;
typedef XID		Drawable;
typedef XID		Font;
typedef XID		Pixmap;
typedef XID		Cursor;
typedef XID		Colormap;
typedef XID		GContext;
typedef XID		KeySym;
typedef unsigned char	KeyCode;

#define None		0L
#define ParentRelative	1L
#define CopyFromParent	0L
#define PointerWindow	0L
#define InputFocus	1L
#define CurrentTime	0L
#define AllocNone	0
#define AllocAll	1

#define Success		0
#define BadAlloc	11

#define NoEventMask		0L
#define KeyPressMask		(1L<<0)
#define KeyReleaseMask		(1L<<1)
#define ButtonPressMask		(1L<<2)
#define ButtonReleaseMask	(1L<<3)
#define EnterWindowMask		(1L<<4)
#define LeaveWindowMask		(1L<<5)
#define PointerMotionMask	(1L<<6)
#define PointerMotionHintMask	(1L<<7)
#define Button1MotionMask	(1L<<8)
#define Button2MotionMask	(1L<<9)
#define Button3MotionMask	(1L<<10)
#define ButtonMotionMask	(1L<<13)
#define ExposureMask		(1L<<15)
#define StructureNotifyMask	(1L<<17)

#define KeyPress		2
#define KeyRelease		3
#define ButtonPress		4
#define ButtonRelease		5
#define MotionNotify		6
#define EnterNotify		7
#define LeaveNotify		8
#define Expose			12
#define ConfigureNotify		22

#define ShiftMask	(1<<0)
#define LockMask	(1<<1)
#define ControlMask	(1<<2)
#define Mod1Mask	(1<<3)
#define Mod2Mask	(1<<4)
#define Mod3Mask	(1<<5)
#define Mod4Mask	(1<<6)
#define Mod5Mask	(1<<7)
#define Button1Mask	(1<<8)
#define Button2Mask	(1<<9)
#define Button3Mask	(1<<10)
#define Button4Mask	(1<<11)
#define Button5Mask	(1<<12)

#define Button1	1
#define Button2	2
#define Button3	3
#define Button4	4
#define Button5	5
#define Button6	6
#define Button7	7

/* graphics functions */
#define GXclear		0x0
#define GXand		0x1
#define GXandReverse	0x2
#define GXcopy		0x3
#define GXandInverted	0x4
#define GXnoop		0x5
#define GXxor		0x6
#define GXor		0x7
#define GXnor		0x8
#define GXequiv		0x9
#define GXinvert	0xa
#define GXorReverse	0xb
#define GXcopyInverted	0xc
#define GXorInverted	0xd
#define GXnand		0xe
#define GXset		0xf

#define LineSolid	0
#define LineOnOffDash	1
#define LineDoubleDash	2
#define CapNotLast	0
#define CapButt		1
#define CapRound	2
#define CapProjecting	3
#define JoinMiter	0
#define JoinRound	1
#define JoinBevel	2
#define FillSolid	0
#define FillTiled	1
#define FillStippled	2
#define FillOpaqueStippled 3
#define EvenOddRule	0
#define WindingRule	1
#define ClipByChildren	0
#define IncludeInferiors 1
#define Unsorted	0
#define YSorted		1
#define YXSorted	2
#define YXBanded	3
#define ArcChord	0
#define ArcPieSlice	1
#define Complex		0
#define Nonconvex	1
#define Convex		2
#define CoordModeOrigin	0
#define CoordModePrevious 1

#define GCFunction	(1L<<0)
#define GCPlaneMask	(1L<<1)
#define GCForeground	(1L<<2)
#define GCBackground	(1L<<3)
#define GCLineWidth	(1L<<4)
#define GCLineStyle	(1L<<5)
#define GCCapStyle	(1L<<6)
#define GCJoinStyle	(1L<<7)
#define GCFillStyle	(1L<<8)
#define GCFillRule	(1L<<9)
#define GCTile		(1L<<10)
#define GCStipple	(1L<<11)
#define GCTileStipXOrigin (1L<<12)
#define GCTileStipYOrigin (1L<<13)
#define GCFont		(1L<<14)
#define GCSubwindowMode	(1L<<15)
#define GCGraphicsExposures (1L<<16)
#define GCClipXOrigin	(1L<<17)
#define GCClipYOrigin	(1L<<18)
#define GCClipMask	(1L<<19)
#define GCDashOffset	(1L<<20)
#define GCDashList	(1L<<21)
#define GCArcMode	(1L<<22)

#define StaticGray	0
#define GrayScale	1
#define StaticColor	2
#define PseudoColor	3
#define TrueColor	4
#define DirectColor	5

#define LSBFirst	0
#define MSBFirst	1
#define XYBitmap	0
#define XYPixmap	1
#define ZPixmap		2

#define DoRed		(1<<0)
#define DoGreen		(1<<1)
#define DoBlue		(1<<2)

#define VisualNoMask		0x0
#define VisualIDMask		0x1
#define VisualScreenMask	0x2
#define VisualClassMask		0x8

/* ---------------------------------------------------------------- Xlib.h */

typedef int	Bool;
typedef int	Status;
#define True	1
#define False	0
#ifndef _XtIntrinsic_h
#endif

typedef char	*XPointer;

typedef struct _XDisplay	Display;
typedef struct _XScreen		Screen;
#ifdef __cplusplus
#define XSHIM_CLASS c_class
#else
#define XSHIM_CLASS class
#endif
typedef struct _XVisual { void *ext_data; VisualID visualid; int XSHIM_CLASS;
	unsigned long red_mask, green_mask, blue_mask; int bits_per_rgb, map_entries; } Visual;
typedef struct _XGC		*GC;
typedef struct _XRegion		*Region;
typedef struct _XIM		*XIM;
typedef struct _XIC		*XIC;
typedef struct _XOC		*XFontSet;
typedef void			*XVaNestedList;
typedef unsigned long		XIMStyle;
typedef struct { unsigned short count_styles; XIMStyle *supported_styles; } XIMStyles;

typedef struct { short x, y; } XPoint;
typedef struct { short x, y; unsigned short width, height; } XRectangle;
typedef struct { short x, y; unsigned short width, height; short angle1, angle2; } XArc;
typedef struct { short x1, y1, x2, y2; } XSegment;

typedef struct {
	unsigned long pixel;
	unsigned short red, green, blue;
	char flags;
	char pad;
} XColor;

typedef struct {
	int function;
	unsigned long plane_mask;
	unsigned long foreground;
	unsigned long background;
	int line_width;
	int line_style;
	int cap_style;
	int join_style;
	int fill_style;
	int fill_rule;
	int arc_mode;
	Pixmap tile;
	Pixmap stipple;
	int ts_x_origin;
	int ts_y_origin;
	Font font;
	int subwindow_mode;
	Bool graphics_exposures;
	int clip_x_origin;
	int clip_y_origin;
	Pixmap clip_mask;
	int dash_offset;
	char dashes;
} XGCValues;

typedef struct {
	short lbearing, rbearing, width, ascent, descent;
	unsigned short attributes;
} XCharStruct;

typedef struct {
	void *ext_data;
	Font fid;
	unsigned direction, min_char_or_byte2, max_char_or_byte2,
		 min_byte1, max_byte1;
	Bool all_chars_exist;
	unsigned default_char;
	int n_properties;
	void *properties;
	XCharStruct min_bounds, max_bounds;
	XCharStruct *per_char;
	int ascent, descent;
} XFontStruct;

typedef struct _XImage {
	int width, height;
	int xoffset;
	int format;
	char *data;
	int byte_order;
	int bitmap_unit;
	int bitmap_bit_order;
	int bitmap_pad;
	int depth;
	int bytes_per_line;
	int bits_per_pixel;
	unsigned long red_mask, green_mask, blue_mask;
	XPointer obdata;
	void *qimage;		/* shim: backing QImage */
} XImage;

typedef struct {
	Visual *visual;
	VisualID visualid;
	int screen;
	int depth;
	int class_;
	unsigned long red_mask, green_mask, blue_mask;
	int colormap_size;
	int bits_per_rgb;
} XVisualInfo;

typedef struct {
	int type; unsigned long serial; Bool send_event; Display *display;
	Window window, root, subwindow; Time time;
	int x, y, x_root, y_root;
	unsigned int state, button;
	Bool same_screen;
} XButtonEvent;

typedef struct {
	int type; unsigned long serial; Bool send_event; Display *display;
	Window window, root, subwindow; Time time;
	int x, y, x_root, y_root;
	unsigned int state, keycode;
	Bool same_screen;
} XKeyEvent;

typedef struct {
	int type; unsigned long serial; Bool send_event; Display *display;
	Window window, root, subwindow; Time time;
	int x, y, x_root, y_root;
	unsigned int state;
	char is_hint;
	Bool same_screen;
} XMotionEvent;

typedef struct {
	int type; unsigned long serial; Bool send_event; Display *display;
	Window window, root, subwindow; Time time;
	int x, y, x_root, y_root;
	int mode, detail;
	Bool same_screen, focus;
	unsigned int state;
} XCrossingEvent;

typedef struct {
	int type; unsigned long serial; Bool send_event; Display *display;
	Window window;
	int x, y, width, height, count;
} XExposeEvent;

typedef struct {
	int type; Display *display; XID resourceid; unsigned long serial;
	unsigned char error_code, request_code, minor_code;
} XErrorEvent;

typedef struct {
	int type; unsigned long serial; Bool send_event; Display *display;
	Window window; Atom message_type; int format;
	union { char b[20]; short s[10]; long l[5]; } data;
} XClientMessageEvent;

typedef struct {
	int type; unsigned long serial; Bool send_event; Display *display;
	Window event, window;
	int x, y, width, height, border_width;
	Window above;
	Bool override_redirect;
} XConfigureEvent;

typedef union _XEvent {
	int type;
	XButtonEvent xbutton;
	XKeyEvent xkey;
	XMotionEvent xmotion;
	XCrossingEvent xcrossing;
	XExposeEvent xexpose;
	XErrorEvent xerror;
	XClientMessageEvent xclient;
	XConfigureEvent xconfigure;
	long pad[24];
} XEvent;

typedef XButtonEvent	XButtonPressedEvent;
typedef XButtonEvent	XButtonReleasedEvent;
typedef XKeyEvent	XKeyPressedEvent;
typedef XMotionEvent	XPointerMovedEvent;

typedef int (*XErrorHandler)(Display *, XErrorEvent *);
typedef int (*XIOErrorHandler)(Display *);

/* drawing (implemented in platform/xlib_qt.cpp) */
extern GC	XCreateGC(Display *, Drawable, unsigned long, XGCValues *);
extern int	XChangeGC(Display *, GC, unsigned long, XGCValues *);
extern int	XCopyGC(Display *, GC, unsigned long, GC);
extern int	XGetGCValues(Display *, GC, unsigned long, XGCValues *);
extern int	XFreeGC(Display *, GC);
extern int	XSetForeground(Display *, GC, unsigned long);
extern int	XSetBackground(Display *, GC, unsigned long);
extern int	XSetFunction(Display *, GC, int);
extern int	XSetLineAttributes(Display *, GC, unsigned int, int, int, int);
extern int	XSetDashes(Display *, GC, int, const char *, int);
extern int	XSetFillStyle(Display *, GC, int);
extern int	XSetStipple(Display *, GC, Pixmap);
extern int	XSetTile(Display *, GC, Pixmap);
extern int	XSetTSOrigin(Display *, GC, int, int);
extern int	XSetFont(Display *, GC, Font);
extern int	XSetClipMask(Display *, GC, Pixmap);
extern int	XSetClipOrigin(Display *, GC, int, int);
extern int	XSetClipRectangles(Display *, GC, int, int, XRectangle *, int, int);
extern int	XSetRegion(Display *, GC, Region);
extern int	XSetPlaneMask(Display *, GC, unsigned long);

extern int	XDrawPoint(Display *, Drawable, GC, int, int);
extern int	XDrawPoints(Display *, Drawable, GC, XPoint *, int, int);
extern int	XDrawLine(Display *, Drawable, GC, int, int, int, int);
extern int	XDrawLines(Display *, Drawable, GC, XPoint *, int, int);
extern int	XDrawSegments(Display *, Drawable, GC, XSegment *, int);
extern int	XDrawRectangle(Display *, Drawable, GC, int, int, unsigned, unsigned);
extern int	XFillRectangle(Display *, Drawable, GC, int, int, unsigned, unsigned);
extern int	XFillRectangles(Display *, Drawable, GC, XRectangle *, int);
extern int	XDrawArc(Display *, Drawable, GC, int, int, unsigned, unsigned, int, int);
extern int	XFillArc(Display *, Drawable, GC, int, int, unsigned, unsigned, int, int);
extern int	XFillPolygon(Display *, Drawable, GC, XPoint *, int, int, int);
extern int	XDrawString(Display *, Drawable, GC, int, int, const char *, int);
extern int	XDrawImageString(Display *, Drawable, GC, int, int, const char *, int);
extern int	XCopyArea(Display *, Drawable, Drawable, GC, int, int, unsigned, unsigned, int, int);
extern int	XCopyPlane(Display *, Drawable, Drawable, GC, int, int, unsigned, unsigned, int, int, unsigned long);
extern int	XClearArea(Display *, Window, int, int, unsigned, unsigned, Bool);
extern int	XClearWindow(Display *, Window);

extern Pixmap	XCreatePixmap(Display *, Drawable, unsigned, unsigned, unsigned);
extern Pixmap	XCreateBitmapFromData(Display *, Drawable, const char *, unsigned, unsigned);
extern Pixmap	XCreatePixmapFromBitmapData(Display *, Drawable, char *, unsigned, unsigned,
				unsigned long, unsigned long, unsigned);
extern int	XReadBitmapFromFile(Display *, Drawable, const char *, unsigned *, unsigned *,
				Pixmap *, int *, int *);
extern int	XFreePixmap(Display *, Pixmap);
extern int	XGetGeometry(Display *, Drawable, Window *, int *, int *, unsigned *,
				unsigned *, unsigned *, unsigned *);

extern XImage	*XCreateImage(Display *, Visual *, unsigned, int, int, char *, unsigned,
				unsigned, int, int);
extern XImage	*XGetImage(Display *, Drawable, int, int, unsigned, unsigned, unsigned long, int);
extern int	XPutImage(Display *, Drawable, GC, XImage *, int, int, int, int, unsigned, unsigned);
extern int	XPutPixel(XImage *, int, int, unsigned long);
extern unsigned long XGetPixel(XImage *, int, int);
extern int	XDestroyImage(XImage *);

extern Region	XCreateRegion(void);
extern Region	XPolygonRegion(XPoint *, int, int);
extern int	XSubtractRegion(Region, Region, Region);
extern int	XUnionRectWithRegion(XRectangle *, Region, Region);
extern int	XDestroyRegion(Region);

extern Status	XAllocColor(Display *, Colormap, XColor *);
extern Status	XParseColor(Display *, Colormap, const char *, XColor *);
extern Status	XLookupColor(Display *, Colormap, const char *, XColor *, XColor *);
extern int	XQueryColor(Display *, Colormap, XColor *);
extern int	XQueryColors(Display *, Colormap, XColor *, int);
extern int	XStoreColor(Display *, Colormap, XColor *);
extern int	XStoreColors(Display *, Colormap, XColor *, int);
extern int	XFreeColors(Display *, Colormap, unsigned long *, int, unsigned long);
extern Status	XAllocColorCells(Display *, Colormap, Bool, unsigned long *, unsigned,
				unsigned long *, unsigned);

extern XFontStruct *XLoadQueryFont(Display *, const char *);
extern XFontStruct *XQueryFont(Display *, XID);
extern int	XFreeFont(Display *, XFontStruct *);
extern int	XTextWidth(XFontStruct *, const char *, int);
extern int	XTextExtents(XFontStruct *, const char *, int, int *, int *, int *, XCharStruct *);
extern char	**XListFonts(Display *, const char *, int, int *);
extern int	XFreeFontNames(char **);

extern int	XDefineCursor(Display *, Window, Cursor);
extern int	XUndefineCursor(Display *, Window);
extern int	XBell(Display *, int);
extern int	XFlush(Display *);
extern int	XSync(Display *, Bool);
extern int	XFree(void *);
extern XErrorHandler	XSetErrorHandler(XErrorHandler);
extern XIOErrorHandler	XSetIOErrorHandler(XIOErrorHandler);
extern int	XGetErrorText(Display *, int, char *, int);
extern int	XGetErrorDatabaseText(Display *, const char *, const char *, const char *, char *, int);
extern XVisualInfo *XGetVisualInfo(Display *, long, XVisualInfo *, int *);
extern int	XWarpPointer(Display *, Window, Window, int, int, unsigned, unsigned, int, int);
extern Bool	XQueryPointer(Display *, Window, Window *, Window *, int *, int *, int *, int *,
				unsigned *);
extern KeySym	XLookupKeysym(XKeyEvent *, int);
extern int	XLookupString(XKeyEvent *, char *, int, KeySym *, void *);
extern char	*XKeysymToString(KeySym);
extern KeySym	XStringToKeysym(const char *);

#define BlackPixelOfScreen(s)	0x000000UL
#define WhitePixelOfScreen(s)	0xffffffUL
#define DefaultScreen(d)	0
#define DefaultDepthOfScreen(s)	24
#define XDisplayWidth(d,s)	1920
#define XDisplayHeight(d,s)	1080

/* ------------------------------------------------------------- Xresource */
typedef struct _XrmHashBucketRec *XrmDatabase;

/* --------------------------------------------------------------- keysyms */
#define XK_BackSpace	0xff08
#define XK_Tab		0xff09
#define XK_Linefeed	0xff0a
#define XK_Return	0xff0d
#define XK_Escape	0xff1b
#define XK_Delete	0xffff
#define XK_Multi_key	0xff20
#define XK_Home		0xff50
#define XK_Left		0xff51
#define XK_Up		0xff52
#define XK_Right	0xff53
#define XK_Down		0xff54
#define XK_Prior	0xff55
#define XK_Next		0xff56
#define XK_End		0xff57
#define XK_KP_Enter	0xff8d
#define XK_F1		0xffbe
#define XK_F35		0xffe0
#define XK_Shift_L	0xffe1
#define XK_Shift_R	0xffe2
#define XK_Control_L	0xffe3
#define XK_Control_R	0xffe4
#define XK_Caps_Lock	0xffe5
#define XK_Meta_L	0xffe7
#define XK_Meta_R	0xffe8
#define XK_Alt_L	0xffe9
#define XK_Alt_R	0xffea
#define XK_space	0x0020

/* -------------------------------------------------------------- Xft / Fc */
typedef FcChar8		XftChar8;
typedef unsigned short	XftChar16;
typedef FcChar32	XftChar32;
typedef unsigned int	FT_UInt;
typedef FcPattern	XftPattern;
typedef FcMatrix	XftMatrix;
typedef FcResult	XftResult;
#define XftResultMatch	FcResultMatch

typedef struct { unsigned short red, green, blue, alpha; } XRenderColor;
typedef struct { unsigned long pixel; XRenderColor color; } XftColor;
typedef struct {
	unsigned short width, height;
	short x, y;
	short xOff, yOff;
} XGlyphInfo;

typedef struct _XftFont {
	int ascent;
	int descent;
	int height;
	int max_advance_width;
	void *charset;
	FcPattern *pattern;
	void *qfont;		/* shim: QFont* */
} XftFont;

typedef struct _XftDraw XftDraw;

#define XFT_FAMILY	FC_FAMILY
#define XFT_SIZE	FC_SIZE
#define XFT_PIXEL_SIZE	FC_PIXEL_SIZE
#define XFT_MATRIX	FC_MATRIX
#define XFT_ANTIALIAS	FC_ANTIALIAS
#define XFT_SCALABLE	FC_SCALABLE
#define XFT_RGBA	FC_RGBA

#define XftMatrixInit(m)	FcMatrixInit(m)
#define XftMatrixRotate(m,c,s)	FcMatrixRotate(m,c,s)
#define XftMatrixScale(m,sx,sy)	FcMatrixScale(m,sx,sy)
#define XftPatternDestroy	FcPatternDestroy
#define XftPatternDuplicate	FcPatternDuplicate
#define XftPatternDel		FcPatternDel
#define XftPatternAddBool	FcPatternAddBool
#define XftPatternAddDouble	FcPatternAddDouble
#define XftPatternAddInteger	FcPatternAddInteger
#define XftPatternAddString	FcPatternAddString
#define XftPatternAddMatrix	FcPatternAddMatrix
#define XftPatternGetDouble	FcPatternGetDouble
#define XftPatternGetString	FcPatternGetString
#define XftPatternGetInteger	FcPatternGetInteger
#define XftNameUnparse		FcNameUnparse
extern FcPattern *XftNameParse(const char *);
extern FcPattern *XftFontMatch(Display *, int, const FcPattern *, FcResult *);
extern XftFont	*XftFontOpenPattern(Display *, FcPattern *);
extern XftFont	*XftFontOpenName(Display *, int, const char *);
extern XftFont	*XftFontCopy(Display *, XftFont *);
extern void	XftFontClose(Display *, XftFont *);
extern FT_UInt	XftCharIndex(Display *, XftFont *, FcChar32);
extern FcBool	XftCharExists(Display *, XftFont *, FcChar32);
extern void	XftTextExtents8(Display *, XftFont *, const FcChar8 *, int, XGlyphInfo *);
extern void	XftTextExtentsUtf8(Display *, XftFont *, const FcChar8 *, int, XGlyphInfo *);
extern void	XftGlyphExtents(Display *, XftFont *, const FT_UInt *, int, XGlyphInfo *);
extern XftDraw	*XftDrawCreate(Display *, Drawable, Visual *, Colormap);
extern void	XftDrawDestroy(XftDraw *);
extern Drawable	XftDrawDrawable(XftDraw *);
extern void	XftDrawChange(XftDraw *, Drawable);
extern void	XftDrawStringUtf8(XftDraw *, const XftColor *, XftFont *, int, int,
				const FcChar8 *, int);
extern void	XftDrawString8(XftDraw *, const XftColor *, XftFont *, int, int,
				const FcChar8 *, int);
extern void	XftDrawGlyphs(XftDraw *, const XftColor *, XftFont *, int, int,
				const FT_UInt *, int);
extern void	XftDrawRect(XftDraw *, const XftColor *, int, int, unsigned, unsigned);
extern Bool	XftDrawSetClipRectangles(XftDraw *, int, int, const XRectangle *, int);
extern Bool	XftDrawSetClip(XftDraw *, Region);
extern Bool	XftColorAllocValue(Display *, Visual *, Colormap, const XRenderColor *, XftColor *);
extern Bool	XftColorAllocName(Display *, const Visual *, Colormap, const char *, XftColor *);
extern void	XftColorFree(Display *, Visual *, Colormap, XftColor *);

/* -------------------------------------------------------------------- Xt */
#ifndef XSHIM_NO_XT
typedef char		Boolean;
typedef char		*String;
typedef unsigned int	Cardinal;
typedef unsigned long	Pixel;
typedef unsigned short	Dimension;
typedef short		Position;
typedef void		*XtPointer;
typedef struct _WidgetRec *Widget;
typedef Widget		*WidgetList;
typedef struct _XtAppStruct *XtAppContext;
typedef unsigned long	XtIntervalId;
typedef unsigned long	XtInputId;
/* holds pointers: intptr_t, as long is 32 bits on 64-bit Windows */
typedef intptr_t	XtArgVal;
typedef struct _TranslationData *XtTranslations;
typedef struct _TranslationData *XtAccelerators;
typedef struct { String name; XtArgVal value; } Arg, *ArgList;
typedef void (*XtCallbackProc)(Widget, XtPointer, XtPointer);
typedef struct { XtCallbackProc callback; XtPointer closure; } XtCallbackRec, *XtCallbackList;
typedef void (*XtActionProc)(Widget, XEvent *, String *, Cardinal *);
typedef struct { String string; XtActionProc proc; } XtActionsRec, *XtActionList;
typedef void (*XtEventHandler)(Widget, XtPointer, XEvent *, Boolean *);
typedef void (*XtTimerCallbackProc)(XtPointer, XtIntervalId *);
typedef void (*XtInputCallbackProc)(XtPointer, int *, XtInputId *);
typedef struct { unsigned int request_mode; Position x, y; Dimension width, height,
	border_width; Widget sibling; int stack_mode; } XtWidgetGeometry;
typedef int XtGrabKind;
#define XtGrabNone		0
#define XtGrabNonexclusive	1
#define XtGrabExclusive		2
#define XtNumber(arr)	((Cardinal)(sizeof(arr) / sizeof(arr[0])))
#define XtSetArg(arg, n, d)	((void)((arg).name = (n), (arg).value = (XtArgVal)(d)))
#define XtJustifyLeft	0
#define XtJustifyCenter	1
#define XtJustifyRight	2
#endif

/* Xt resource names */
#define XtNaccelerators          "accelerators"
#define XtNallowHoriz            "allowHoriz"
#define XtNallowShellResize      "allowShellResize"
#define XtNallowVert             "allowVert"
#define XtNargc                  "argc"
#define XtNargv                  "argv"
#define XtNbackground            "background"
#define XtNbackgroundPixmap      "backgroundPixmap"
#define XtNbitmap                "bitmap"
#define XtNborder                "border"
#define XtNborderColor           "borderColor"
#define XtNborderWidth           "borderWidth"
#define XtNbottom                "bottom"
#define XtNbottomMargin          "bottomMargin"
#define XtNcallback              "callback"
#define XtNcolormap              "colormap"
#define XtNdefaultColumns        "defaultColumns"
#define XtNdefaultDistance       "defaultDistance"
#define XtNdepth                 "depth"
#define XtNdestroyCallback       "destroyCallback"
#define XtNdisplayCaret          "displayCaret"
#define XtNdisplayPosition       "displayPosition"
#define XtNeditType              "editType"
#define XtNfont                  "font"
#define XtNforceBars             "forceBars"
#define XtNforceColumns          "forceColumns"
#define XtNforeground            "foreground"
#define XtNfromHoriz             "fromHoriz"
#define XtNfromVert              "fromVert"
#define XtNheight                "height"
#define XtNhorizDistance         "horizDistance"
#define XtNhSpace                "hSpace"
#define XtNiconPixmap            "iconPixmap"
#define XtNiconWindow            "iconWindow"
#define XtNiconX                 "iconX"
#define XtNiconY                 "iconY"
#define XtNinput                 "input"
#define XtNinsertPosition        "insertPosition"
#define XtNinternalHeight        "internalHeight"
#define XtNinternalWidth         "internalWidth"
#define XtNinternational         "international"
#define XtNjumpProc              "jumpProc"
#define XtNjustify               "justify"
#define XtNlabel                 "label"
#define XtNleft                  "left"
#define XtNleftBitmap            "leftBitmap"
#define XtNleftMargin            "leftMargin"
#define XtNlineWidth             "lineWidth"
#define XtNlist                  "list"
#define XtNmappedWhenManaged     "mappedWhenManaged"
#define XtNmenuName              "menuName"
#define XtNnumberStrings         "numberStrings"
#define XtNorientation           "orientation"
#define XtNpasteBuffer           "pasteBuffer"
#define XtNradioData             "radioData"
#define XtNradioGroup            "radioGroup"
#define XtNresizable             "resizable"
#define XtNresize                "resize"
#define XtNright                 "right"
#define XtNrightBitmap           "rightBitmap"
#define XtNscrollHorizontal      "scrollHorizontal"
#define XtNscrollProc            "scrollProc"
#define XtNscrollVertical        "scrollVertical"
#define XtNsensitive             "sensitive"
#define XtNstate                 "state"
#define XtNstring                "string"
#define XtNtextSource            "textSource"
#define XtNthickness             "thickness"
#define XtNthumb                 "thumb"
#define XtNtip                   "tip"
#define XtNtitle                 "title"
#define XtNtitleEncoding         "titleEncoding"
#define XtNtop                   "top"
#define XtNtopMargin             "topMargin"
#define XtNtranslations          "translations"
#define XtNuseBottom             "useBottom"
#define XtNvertDistance          "vertDistance"
#define XtNverticalList          "verticalList"
#define XtNvertSpace             "vertSpace"
#define XtNvisual                "visual"
#define XtNvSpace                "vSpace"
#define XtNwidth                 "width"
#define XtNx                     "x"
#define XtNy                     "y"
#define XtRBoolean               "boolean"
#define XtRFloat                 "float"
#define XtRFontSet               "fontSet"
#define XtRImmediate             "immediate"
#define XtRInt                   "int"
#define XtRString                "string"
#define XtRVisual                "visual"

#ifndef XSHIM_NO_XT
#ifndef TRUE
#define TRUE	1
#define FALSE	0
#endif
typedef struct _WidgetClassRec *WidgetClass;
typedef struct { String resource_name, resource_class, resource_type; Cardinal resource_size,
	resource_offset; String default_type; XtPointer default_addr; } XtResource;
#define XtChainTop	0
#define XtChainBottom	1
#define XtChainLeft	2
#define XtChainRight	3
#define XtRubber	4
#define XawtextRead	0
#define XawtextAppend	1
#define XawtextEdit	2
#define XawtextScrollNever	0
#define XawtextScrollWhenNeeded	1
#define XawtextScrollAlways	2
#define XtOffsetOf(s, f)	offsetof(s, f)
extern void	XtSetValues(Widget, ArgList, Cardinal);
extern void	XtGetValues(Widget, ArgList, Cardinal);
extern void	XtSetSensitive(Widget, Boolean);
extern Boolean	XtIsSensitive(Widget);
extern Boolean	XtIsRealized(Widget);
extern Boolean	XtIsManaged(Widget);
extern Window	XtWindow(Widget);
extern Display	*XtDisplay(Widget);
extern String	XtName(Widget);
extern Widget	XtParent(Widget);
extern void	XtManageChild(Widget);
extern void	XtUnmanageChild(Widget);
extern void	XtDestroyWidget(Widget);
extern void	XtPopup(Widget, XtGrabKind);
extern void	XtPopdown(Widget);
extern char	*XtMalloc(Cardinal);
extern char	*XtCalloc(Cardinal, Cardinal);
extern char	*XtRealloc(char *, Cardinal);
extern void	XtFree(char *);
extern String	XtNewString(const char *);
extern XtIntervalId XtAppAddTimeOut(XtAppContext, unsigned long, XtTimerCallbackProc, XtPointer);
extern void	XtRemoveTimeOut(XtIntervalId);
extern void	XtAppProcessEvent(XtAppContext, unsigned long);
extern Boolean	XtAppPending(XtAppContext);
extern void	XtAddCallback(Widget, String, XtCallbackProc, XtPointer);
#define XtIMAll	7
#endif

/* Xutil.h / Xmd.h / Xlib macros */
#define BitmapSuccess		0
#define BitmapOpenFailed	1
#define BitmapFileInvalid	2
#define BitmapNoMemory		3
typedef uint32_t CARD32;
typedef uint16_t CARD16;
typedef uint8_t	 CARD8;
#define ClientMessage	33
#define XA_STRING	((Atom)31)
extern GC	XDefaultGC(Display *, int);
extern Window	XDefaultRootWindow(Display *);
#define DefaultGC(d, s)		XDefaultGC(d, s)
#define DefaultRootWindow(d)	XDefaultRootWindow(d)
#define RootWindowOfScreen(s)	XDefaultRootWindow(NULL)
#define _XInitImageFuncPtrs(img)	((void)0)

#ifndef XSHIM_NO_XT
#include "xtshim.h"
#endif

#ifdef __cplusplus
}
#endif

#endif /* XSHIM_H */
