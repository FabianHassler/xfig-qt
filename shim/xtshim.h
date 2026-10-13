/*
 * xtshim.h - declarations of the Xt / Xaw / misc Xlib API used by xfig's
 * widget code.  They are implemented as inert stubs in xt_null.c: widgets
 * are created as empty records, nothing is shown.  This lets xfig's panel
 * modules compile and keep their logic while the Qt user interface replaces
 * them one at a time.  Included from xshim.h.
 */
#ifndef XTSHIM_H
#define XTSHIM_H

typedef unsigned long	EventMask;
typedef int		XtOrientation;
#define XtorientHorizontal	0
#define XtorientVertical	1
typedef XtResource	*XtResourceList;
typedef struct { unsigned int size; XPointer addr; } XrmValue, *XrmValuePtr;
typedef struct { int address_mode; XPointer address_id; Cardinal size; }
			XtConvertArgRec, *XtConvertArgList;
typedef void (*XtConverter)(XrmValue *, Cardinal *, XrmValue *, XrmValue *);
typedef void (*XtSelectionCallbackProc)(Widget, XtPointer, Atom *, Atom *,
			XtPointer, unsigned long *, int *);
typedef enum { XrmoptionNoArg, XrmoptionIsArg, XrmoptionStickyArg,
	XrmoptionSepArg, XrmoptionResArg, XrmoptionSkipArg, XrmoptionSkipLine,
	XrmoptionSkipNArgs } XrmOptionKind;
typedef struct { char *option; char *specifier; XrmOptionKind argKind;
	XPointer value; } XrmOptionDescRec, *XrmOptionDescList;
typedef void *(*XtLanguageProc)(Display *, String, XtPointer);

#define XtOffset(p_type, field) \
	((Cardinal)(((char *)(&(((p_type)NULL)->field))) - ((char *)NULL)))

#define XtCWidth	"Width"
#define XtCHeight	"Height"
#define XtCJustify	"Justify"
#define XtCOrientation	"Orientation"
#define XtCGeometry	"Geometry"

/* widget classes */
extern WidgetClass	applicationShellWidgetClass, transientShellWidgetClass,
			overrideShellWidgetClass, topLevelShellWidgetClass,
			labelWidgetClass, commandWidgetClass, formWidgetClass,
			menuButtonWidgetClass, asciiTextWidgetClass,
			toggleWidgetClass, viewportWidgetClass, smeBSBObjectClass,
			smeObjectClass, smeLineObjectClass, scrollbarWidgetClass,
			boxWidgetClass, simpleMenuWidgetClass, listWidgetClass,
			panedWidgetClass, dialogWidgetClass, simpleWidgetClass,
			coreWidgetClass, widgetClass;

/* Xt */
extern Widget	XtCreateManagedWidget(const char *, WidgetClass, Widget, ArgList, Cardinal);
extern Widget	XtCreateWidget(const char *, WidgetClass, Widget, ArgList, Cardinal);
extern Widget	XtCreatePopupShell(const char *, WidgetClass, Widget, ArgList, Cardinal);
extern Widget	XtVaCreateManagedWidget(const char *, WidgetClass, Widget, ...);
extern Widget	XtVaCreateWidget(const char *, WidgetClass, Widget, ...);
extern Widget	XtVaCreatePopupShell(const char *, WidgetClass, Widget, ...);
extern Widget	XtAppCreateShell(const char *, const char *, WidgetClass, Display *,
			ArgList, Cardinal);
extern Widget	XtAppInitialize(XtAppContext *, const char *, XrmOptionDescList, Cardinal,
			int *, String *, String *, ArgList, Cardinal);
extern void	XtVaSetValues(Widget, ...);
extern void	XtVaGetValues(Widget, ...);
extern XtTranslations XtParseTranslationTable(const char *);
extern XtAccelerators XtParseAcceleratorTable(const char *);
extern void	XtOverrideTranslations(Widget, XtTranslations);
extern void	XtAugmentTranslations(Widget, XtTranslations);
extern void	XtAppAddActions(XtAppContext, XtActionList, Cardinal);
extern void	XtGetActionList(WidgetClass, XtActionList *, Cardinal *);
extern void	XtAddEventHandler(Widget, EventMask, Boolean, XtEventHandler, XtPointer);
extern void	XtRemoveEventHandler(Widget, EventMask, Boolean, XtEventHandler, XtPointer);
extern void	XtTranslateCoords(Widget, Position, Position, Position *, Position *);
extern void	XtInstallAccelerators(Widget, Widget);
extern void	XtInstallAllAccelerators(Widget, Widget);
extern void	XtRealizeWidget(Widget);
extern void	XtResizeWidget(Widget, Dimension, Dimension, Dimension);
extern Widget	XtNameToWidget(Widget, const char *);
extern Boolean	XtDispatchEvent(XEvent *);
extern void	XtAppNextEvent(XtAppContext, XEvent *);
extern void	XtAppMainLoop(XtAppContext);
extern void	XtSetKeyboardFocus(Widget, Widget);
extern XtAppContext XtWidgetToApplicationContext(Widget);
extern void	XtWarning(const char *);
extern void	XtMapWidget(Widget);
extern void	XtUnmapWidget(Widget);
extern Screen	*XtScreen(Widget);
extern void	XtPopupSpringLoaded(Widget);
extern void	XtGetSelectionValue(Widget, Atom, Atom, XtSelectionCallbackProc,
			XtPointer, Time);
extern void	XtGetApplicationResources(Widget, XtPointer, XtResourceList, Cardinal,
			ArgList, Cardinal);
extern void	XtStringConversionWarning(const char *, const char *);
extern XtLanguageProc XtSetLanguageProc(XtAppContext, XtLanguageProc, XtPointer);
extern void	XtManageChildren(WidgetList, Cardinal);
extern void	XtUnmanageChildren(WidgetList, Cardinal);
extern int	XtMakeResizeRequest(Widget, Dimension, Dimension, Dimension *, Dimension *);
extern int	XtMakeGeometryRequest(Widget, XtWidgetGeometry *, XtWidgetGeometry *);
extern void	XtCallCallbacks(Widget, const char *, XtPointer);
extern void	XtRemoveAllCallbacks(Widget, const char *);
extern void	XtAppAddConverter(XtAppContext, const char *, const char *, XtConverter,
			XtConvertArgList, Cardinal);
extern void	XtAddGrab(Widget, Boolean, Boolean);
extern void	XtRemoveGrab(Widget);
extern XtInputId XtAppAddInput(XtAppContext, int, XtPointer, XtInputCallbackProc, XtPointer);
extern void	XtRemoveInput(XtInputId);
extern void	XtAppSetErrorHandler(XtAppContext, void (*)(String));
extern void	XtAppSetWarningHandler(XtAppContext, void (*)(String));

/* Xaw */
typedef long	XawTextPosition;
typedef struct { int firstPos; int length; char *ptr; unsigned long format; } XawTextBlock;
#define FMT8BIT		0L
#define XawEditDone	0
#define XawEditError	1
#define XawPositionError 2
#define XawChainTop	XtChainTop
#define XawChainBottom	XtChainBottom
#define XawChainLeft	XtChainLeft
#define XawChainRight	XtChainRight
#define XawtextScrollNever	0
/* Cardinals.h */
#define ZERO	((Cardinal)0)
#define ONE	((Cardinal)1)
#define TWO	((Cardinal)2)
#define THREE	((Cardinal)3)
#define FOUR	((Cardinal)4)
#define FIVE	((Cardinal)5)
typedef struct { String string; int list_index; } XawListReturnStruct;
#define XAW_LIST_NONE	-1
extern void	XawTipEnable(Widget);
extern void	XawTipDisable(Widget);
extern void	XawScrollbarSetThumb(Widget, float, float);
extern void	XawTextSetInsertionPoint(Widget, XawTextPosition);
extern XawTextPosition XawTextGetInsertionPoint(Widget);
extern int	XawTextReplace(Widget, XawTextPosition, XawTextPosition, XawTextBlock *);
extern XawTextPosition XawTextSourceScan(Widget, XawTextPosition, int, int, int, Boolean);
extern Widget	XawTextGetSource(Widget);
extern void	XawTextDisplay(Widget);
extern void	XawTextSetSelection(Widget, XawTextPosition, XawTextPosition);
extern void	XawFormDoLayout(Widget, Boolean);
extern void	XawListChange(Widget, String *, int, int, Boolean);
extern void	XawListHighlight(Widget, int);
extern void	XawListUnhighlight(Widget);
extern XawListReturnStruct *XawListShowCurrent(Widget);
extern XtPointer XawToggleGetCurrent(Widget);
extern void	XawToggleSetCurrent(Widget, XtPointer);
extern void	XawToggleUnsetCurrent(Widget);
extern void	XawSimpleMenuAddGlobalActions(XtAppContext);
extern void	XawAsciiSourceFreeString(Widget);
#define XawsdLeft	0
#define XawsdRight	1
#define XawstEOL	2
#define XawstAll	5

/* more Xlib */
#define XA_PRIMARY	((Atom)1)
#define XA_ATOM		((Atom)4)
#define XA_WINDOW	((Atom)33)
#define GrabModeSync	0
#define GrabModeAsync	1
#define GrabSuccess	0
#define AllPlanes	(~0UL)
#define AsyncPointer	0
#define SyncPointer	1
#define ReplayPointer	2
#define InputOutput	1
#define InputOnly	2
#define InputHint	(1L << 0)
#define StateHint	(1L << 1)
#define IconPixmapHint	(1L << 2)
#define IconWindowHint	(1L << 3)
#define CWX		(1 << 0)
#define CWY		(1 << 1)
#define CWWidth		(1 << 2)
#define CWHeight	(1 << 3)
#define NormalState	1
#define IconicState	3
#define XBufferOverflow	-1
#define XLookupNone	1
#define XLookupChars	2
#define XLookupKeySym	3
#define XLookupBoth	4
#define IsViewable	2
#define IsUnmapped	0

/* cursorfont.h */
#define XC_circle		24
#define XC_crosshair		34
#define XC_dotbox		40
#define XC_hand1		58
#define XC_hand2		60
#define XC_left_ptr		68
#define XC_icon			72
#define XC_pirate		88
#define XC_sb_down_arrow	106
#define XC_sb_h_double_arrow	108
#define XC_sb_left_arrow	110
#define XC_sb_right_arrow	112
#define XC_sb_up_arrow		114
#define XC_sb_v_double_arrow	116
#define XC_tcross		130
#define XC_watch		150
#define XC_xterm		152

typedef struct {
	int x, y, width, height, border_width, depth;
	Visual *visual;
	Window root;
	int XSHIM_CLASS, bit_gravity, win_gravity, backing_store;
	unsigned long backing_planes, backing_pixel;
	Bool save_under;
	Colormap colormap;
	Bool map_installed;
	int map_state;
	long all_event_masks, your_event_mask, do_not_propagate_mask;
	Bool override_redirect;
	Screen *screen;
} XWindowAttributes;
typedef struct {
	long flags; Bool input; int initial_state; Pixmap icon_pixmap;
	Window icon_window; int icon_x, icon_y; Pixmap icon_mask; XID window_group;
} XWMHints;
typedef struct { unsigned char *value; Atom encoding; int format; unsigned long nitems; } XTextProperty;
typedef struct { int depth, bits_per_pixel, scanline_pad; } XPixmapFormatValues;

#define XNPreeditAttributes	"preeditAttributes"
#define XNStatusAttributes	"statusAttributes"
#define XNSpotLocation		"spotLocation"
#define XNInputStyle		"inputStyle"
#define XNFontSet		"fontSet"
#define XNQueryInputStyle	"queryInputStyle"
#define XNFocusWindow		"focusWindow"
#define XNClientWindow		"clientWindow"
#define XNAreaNeeded		"areaNeeded"
#define XNArea			"area"
#define XIMPreeditArea		0x0001L
#define XIMPreeditCallbacks	0x0002L
#define XIMPreeditPosition	0x0004L
#define XIMPreeditNothing	0x0008L
#define XIMPreeditNone		0x0010L
#define XIMStatusArea		0x0100L
#define XIMStatusCallbacks	0x0200L
#define XIMStatusNothing	0x0400L
#define XIMStatusNone		0x0800L

extern Atom	XInternAtom(Display *, const char *, Bool);
extern int	XRaiseWindow(Display *, Window);
extern int	XUngrabPointer(Display *, Time);
extern int	XGrabPointer(Display *, Window, Bool, unsigned int, int, int, Window,
			Cursor, Time);
extern int	XAllowEvents(Display *, int, Time);
extern Bool	XTranslateCoordinates(Display *, Window, Window, int, int, int *, int *,
			Window *);
extern Status	XGetWindowAttributes(Display *, Window, XWindowAttributes *);
extern Cursor	XCreateFontCursor(Display *, unsigned int);
extern Cursor	XCreatePixmapCursor(Display *, Pixmap, Pixmap, XColor *, XColor *,
			unsigned, unsigned);
extern int	XRecolorCursor(Display *, Cursor, XColor *, XColor *);
extern int	XSynchronize(Display *, Bool);
extern int	XSetWMProtocols(Display *, Window, Atom *, int);
extern int	XSetWMHints(Display *, Window, XWMHints *);
extern XWMHints	*XGetWMHints(Display *, Window);
extern int	XSetIconName(Display *, Window, const char *);
extern int	XSetWindowColormap(Display *, Window, Colormap);
extern int	XSetSubwindowMode(Display *, GC, int);
extern char	*XSetLocaleModifiers(const char *);
extern Status	XSendEvent(Display *, Window, Bool, long, XEvent *);
extern int	XPutBackEvent(Display *, XEvent *);
extern int	XNextEvent(Display *, XEvent *);
extern Bool	XFilterEvent(XEvent *, Window);
extern XPixmapFormatValues *XListPixmapFormats(Display *, int *);
extern Colormap	*XListInstalledColormaps(Display *, Window, int *);
extern Colormap	XCreateColormap(Display *, Window, Visual *, int);
extern Colormap	XCopyColormapAndFree(Display *, Colormap);
extern int	XInstallColormap(Display *, Colormap);
extern int	XGetPointerMapping(Display *, unsigned char *, int);
extern int	XGeometry(Display *, int, const char *, const char *, unsigned, unsigned,
			unsigned, int, int, int *, int *, int *, int *);
extern VisualID	XVisualIDFromVisual(Visual *);
extern int	XResizeWindow(Display *, Window, unsigned, unsigned);
extern int	XMoveWindow(Display *, Window, int, int);
extern int	XMapWindow(Display *, Window);
extern int	XMapRaised(Display *, Window);
extern int	XUnmapWindow(Display *, Window);
extern Window	XCreateSimpleWindow(Display *, Window, int, int, unsigned, unsigned,
			unsigned, unsigned long, unsigned long);
extern int	XSetWindowBackgroundPixmap(Display *, Window, Pixmap);
extern int	XSetWindowBackground(Display *, Window, unsigned long);
extern int	XStoreName(Display *, Window, const char *);
extern int	XFreeCursor(Display *, Cursor);
extern Bool	XCheckTypedEvent(Display *, int, XEvent *);
extern Bool	XCheckMaskEvent(Display *, long, XEvent *);
extern Bool	XCheckWindowEvent(Display *, Window, long, XEvent *);
extern int	XPending(Display *);

/* input methods */
extern XIM	XOpenIM(Display *, XrmDatabase, char *, char *);
extern Status	XCloseIM(XIM);
extern char	*XGetIMValues(XIM, ...);
extern XIC	XCreateIC(XIM, ...);
extern void	XDestroyIC(XIC);
extern char	*XSetICValues(XIC, ...);
extern char	*XGetICValues(XIC, ...);
extern void	XSetICFocus(XIC);
extern void	XUnsetICFocus(XIC);
extern XVaNestedList XVaCreateNestedList(int, ...);
extern int	Xutf8LookupString(XIC, XKeyPressedEvent *, char *, int, KeySym *, Status *);
extern int	Xutf8TextPropertyToTextList(Display *, const XTextProperty *, char ***, int *);
extern void	XFreeStringList(char **);
extern XFontSet	XCreateFontSet(Display *, const char *, char ***, int *, char **);
extern void	XFreeFontSet(Display *, XFontSet);

#define XftUtf8ToUcs4(s, d, l)	FcUtf8ToUcs4(s, d, l)
#define XftUtf8Len(s, l, n, w)	FcUtf8Len(s, l, n, w)

/* screen macros: the "screen" is the Qt primary screen */
extern int	xq_screen_width(void), xq_screen_height(void);
extern int	xq_screen_width_mm(void), xq_screen_height_mm(void);
extern Visual	*xq_default_visual(void);
#define WidthOfScreen(s)	xq_screen_width()
#define HeightOfScreen(s)	xq_screen_height()
#define WidthMMOfScreen(s)	xq_screen_width_mm()
#define HeightMMOfScreen(s)	xq_screen_height_mm()
#define CellsOfScreen(s)	256
#define ScreenOfDisplay(d, n)	((Screen *)(d))
#define DefaultScreenOfDisplay(d) ((Screen *)(d))
#define DefaultVisual(d, s)	xq_default_visual()
#define DefaultVisualOfScreen(s) xq_default_visual()
#define DefaultColormap(d, s)	((Colormap)1)
#define DefaultColormapOfScreen(s) ((Colormap)1)
#define DefaultDepth(d, s)	24
#define DisplayOfScreen(s)	((Display *)(s))

#endif /* XTSHIM_H */
