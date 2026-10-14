/*
 * xt_qt.cpp - the parts of Xt/Xlib that need the Qt event loop:
 * timers, event processing, pointer queries, keyboard lookup, cursors and
 * the screen.
 */
#include <QApplication>
#include <QBitmap>
#include <QCursor>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QHash>
#include <QScreen>
#include <QSocketNotifier>
#include <QTimer>

#if defined(__APPLE__) || defined(__GLIBC__)
#include <execinfo.h>
#define HAVE_BACKTRACE 1
#endif
#include <unistd.h>

#include "xq.h"
#include "xq_widget.h"

/* ---------------------------------------------------------------- timers */

static QHash<XtIntervalId, QTimer *> timers;
static XtIntervalId next_timer = 1;

extern "C" XtIntervalId
XtAppAddTimeOut(XtAppContext, unsigned long ms, XtTimerCallbackProc proc,
		XtPointer data)
{
	XtIntervalId id = next_timer++;
	QTimer *t = new QTimer;
	t->setSingleShot(true);
	QObject::connect(t, &QTimer::timeout, [id, proc, data, t]() {
		timers.remove(id);
		t->deleteLater();
		XtIntervalId copy = id;
		proc(data, &copy);
	});
	timers.insert(id, t);
	t->start(int(ms));
	return id;
}

extern "C" void
XtRemoveTimeOut(XtIntervalId id)
{
	QTimer *t = timers.take(id);
	if (t) {
		t->stop();
		t->deleteLater();
	}
}

/* ---------------------------------------------------------------- inputs */

static QHash<XtInputId, QSocketNotifier *> inputs;
static XtInputId next_input = 1;

extern "C" XtInputId
XtAppAddInput(XtAppContext, int fd, XtPointer cond, XtInputCallbackProc proc,
		XtPointer data)
{
	(void)cond;
	XtInputId id = next_input++;
	QSocketNotifier *n = new QSocketNotifier(fd, QSocketNotifier::Read);
	QObject::connect(n, &QSocketNotifier::activated, [id, proc, data, fd]() {
		int src = fd;
		XtInputId copy = id;
		proc(data, &src, &copy);
	});
	inputs.insert(id, n);
	return id;
}

extern "C" void
XtRemoveInput(XtInputId id)
{
	QSocketNotifier *n = inputs.take(id);
	if (n) {
		n->setEnabled(false);
		n->deleteLater();
	}
}

/* ---------------------------------------------------------------- events */

/*
 * xfig drains the queue with "while (XtAppPending()) XtAppProcessEvent();".
 * Qt cannot tell whether events are pending, so report "pending" every
 * other call: each such loop then processes the Qt queue exactly once.
 */
extern "C" Boolean
XtAppPending(XtAppContext)
{
	static bool toggle;
	toggle = !toggle;
	return toggle && QCoreApplication::instance() != nullptr;
}

extern "C" void
XtAppProcessEvent(XtAppContext, unsigned long)
{
	QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
}

extern "C" void
XtAppNextEvent(XtAppContext, XEvent *e)
{
	QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
	memset(e, 0, sizeof *e);
}

/*
 * xfig waits for answers to its dialogs with
 *	while (!done) { XNextEvent(); XtDispatchEvent(); }
 * so XNextEvent must run the Qt event loop until something happens.
 */
extern "C" int
XNextEvent(Display *, XEvent *e)
{
	QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
	memset(e, 0, sizeof *e);
	return 1;
}

extern "C" void
XtAppMainLoop(XtAppContext)
{
	QApplication::exec();
}

/* --------------------------------------------------------- pointer, keys */

static Window	ptr_window;
static int	ptr_x, ptr_y;
static unsigned	ptr_state;
static QByteArray key_text;
static KeySym	key_sym;

extern "C" void
xq_set_pointer(Window w, int x, int y, unsigned int state)
{
	ptr_window = w;
	ptr_x = x;
	ptr_y = y;
	ptr_state = state;
}

extern "C" void
xq_set_key(const char *utf8, KeySym sym)
{
	key_text = utf8 ? QByteArray(utf8) : QByteArray();
	key_sym = sym;
}

extern "C" Bool
XQueryPointer(Display *, Window w, Window *root, Window *child, int *rx, int *ry,
		int *wx, int *wy, unsigned *mask)
{
	if (root) *root = 0;
	if (child) *child = 0;
	if (rx) *rx = ptr_x;
	if (ry) *ry = ptr_y;
	if (wx) *wx = ptr_x;
	if (wy) *wy = ptr_y;
	if (mask) *mask = ptr_state;
	return w == ptr_window || ptr_window == 0;
}

extern "C" int
XLookupString(XKeyEvent *, char *buf, int n, KeySym *sym, void *)
{
	int len = qMin<int>(n, key_text.size());
	memcpy(buf, key_text.constData(), len);
	if (len < n)
		buf[len] = '\0';
	if (sym)
		*sym = key_sym;
	return len;
}

extern "C" KeySym
XLookupKeysym(XKeyEvent *, int)
{
	return key_sym;
}

static const struct { const char *name; KeySym sym; } keynames[] = {
	{"BackSpace", XK_BackSpace}, {"Tab", XK_Tab}, {"Linefeed", XK_Linefeed},
	{"Return", XK_Return}, {"Escape", XK_Escape}, {"Delete", XK_Delete},
	{"Multi_key", XK_Multi_key}, {"Home", XK_Home}, {"Left", XK_Left},
	{"Up", XK_Up}, {"Right", XK_Right}, {"Down", XK_Down},
	{"Prior", XK_Prior}, {"Next", XK_Next}, {"End", XK_End},
	{"KP_Enter", XK_KP_Enter}, {"Shift_L", XK_Shift_L},
	{"Shift_R", XK_Shift_R}, {"Control_L", XK_Control_L},
	{"Control_R", XK_Control_R}, {"Caps_Lock", XK_Caps_Lock},
	{"Meta_L", XK_Meta_L}, {"Meta_R", XK_Meta_R}, {"Alt_L", XK_Alt_L},
	{"Alt_R", XK_Alt_R}, {"space", XK_space},
};

extern "C" char *
XKeysymToString(KeySym s)
{
	static char buf[8];
	if (s >= XK_F1 && s <= XK_F35) {
		snprintf(buf, sizeof buf, "F%d", int(s - XK_F1 + 1));
		return buf;
	}
	for (auto &k : keynames)
		if (k.sym == s)
			return (char *)k.name;
	if (s >= 0x20 && s < 0x7f) {
		buf[0] = (char)s;
		buf[1] = '\0';
		return buf;
	}
	return nullptr;
}

extern "C" KeySym
XStringToKeysym(const char *n)
{
	for (auto &k : keynames)
		if (strcmp(k.name, n) == 0)
			return k.sym;
	if (n[0] == 'F' && n[1] >= '1' && n[1] <= '9') {
		int f = atoi(n + 1);
		if (f >= 1 && f <= 35)
			return XK_F1 + f - 1;
	}
	if (n[0] && !n[1])
		return (unsigned char)n[0];
	return 0;
}

/* --------------------------------------------------------------- cursors */

static QHash<Cursor, QCursor> pixmap_cursors;
static Cursor next_cursor = 0x2000;

extern "C" Cursor
XCreateFontCursor(Display *, unsigned int shape)
{
	return 0x1000 + shape;
}

extern "C" Cursor
XCreatePixmapCursor(Display *, Pixmap src, Pixmap mask, XColor *, XColor *,
		unsigned x, unsigned y)
{
	QImage *s = xq_image(src);
	if (!s)
		return XCreateFontCursor(nullptr, XC_left_ptr);
	QImage m = mask && xq_image(mask) ? *xq_image(mask) : *s;
	QBitmap bm = QBitmap::fromImage(s->createMaskFromColor(0xff000000u,
				Qt::MaskInColor));
	QBitmap mk = QBitmap::fromImage(m.createMaskFromColor(0xff000000u,
				Qt::MaskInColor));
	Cursor id = next_cursor++;
	pixmap_cursors.insert(id, QCursor(bm, mk, int(x), int(y)));
	return id;
}

QCursor
xq_qcursor(Cursor c)
{
	if (pixmap_cursors.contains(c))
		return pixmap_cursors.value(c);
	switch (c >= 0x1000 && c < 0x2000 ? int(c - 0x1000) : -1) {
	case XC_crosshair:
	case XC_tcross:		return QCursor(Qt::CrossCursor);
	case XC_xterm:		return QCursor(Qt::IBeamCursor);
	case XC_watch:		return QCursor(Qt::WaitCursor);
	case XC_sb_v_double_arrow: return QCursor(Qt::SizeVerCursor);
	case XC_sb_h_double_arrow: return QCursor(Qt::SizeHorCursor);
	case XC_sb_up_arrow:
	case XC_sb_down_arrow:	return QCursor(Qt::SizeVerCursor);
	case XC_sb_left_arrow:
	case XC_sb_right_arrow:	return QCursor(Qt::SizeHorCursor);
	case XC_pirate:		return QCursor(Qt::ForbiddenCursor);
	case XC_hand1:
	case XC_hand2:		return QCursor(Qt::PointingHandCursor);
	case XC_dotbox:
	case XC_circle:		return QCursor(Qt::CrossCursor);
	default:		return QCursor(Qt::ArrowCursor);
	}
}

/* ---------------------------------------------------------------- screen */

static QScreen *
screen()
{
	return QGuiApplication::primaryScreen();
}

extern "C" int xq_screen_width(void) { return screen() ? screen()->geometry().width() : 1920; }
extern "C" int xq_screen_height(void) { return screen() ? screen()->geometry().height() : 1080; }
extern "C" int xq_screen_width_mm(void) { return screen() ? int(screen()->physicalSize().width()) : 508; }
extern "C" int xq_screen_height_mm(void) { return screen() ? int(screen()->physicalSize().height()) : 286; }

extern "C" Visual *
xq_default_visual(void)
{
	static Visual v = {nullptr, 0x21, TrueColor, 0xff0000, 0x00ff00, 0x0000ff, 8, 256};
	return &v;
}

/* see CMakeLists.txt: xfig's exit() */
extern "C" void
xq_exit(int status)
{
#ifdef HAVE_BACKTRACE
	if (qEnvironmentVariableIsSet("XFIGQT_DEBUG")) {
		void *frames[32];
		int n = backtrace(frames, 32);
		fprintf(stderr, "xfig exits with status %d from:\n", status);
		backtrace_symbols_fd(frames, n, 2);
	}
#endif
	fflush(nullptr);
	_exit(status);
}
