/*
 * xaw.cpp - Qt peers for xfig's Xaw widgets.
 */
#include "xaw.h"
#include "bridge.h"

static QHash<Window, XawPeer *> by_window;
static QSet<XawPeer *> toggles;
static int ui_font_px = 12;

static QFont
ui_font()
{
	QFont f = QApplication::font();
	f.setPixelSize(ui_font_px);
	return f;
}

static bool
is(Widget w, const char *cls)
{
	return !strcmp(xq_widget_class(w), cls);
}

static bool
is_shell(Widget w)
{
	const char *c = xq_widget_class(w);
	return !strcmp(c, "ApplicationShell") || !strcmp(c, "TransientShell") ||
		!strcmp(c, "OverrideShell") || !strcmp(c, "TopLevelShell");
}

static bool
is_menu_object(Widget w)
{
	return is(w, "SmeBSB") || is(w, "Sme") || is(w, "SmeLine") ||
		is(w, "SimpleMenu");
}

static XtArgVal
res(Widget w, const char *name, XtArgVal def = 0)
{
	return xq_res_default(w, name, def);
}

static QString
qstr(const char *s)
{
	return s ? QString::fromUtf8(s) : QString();
}

/* -------------------------------------------------------------- XawPeer */

XawPeer *
XawPeer::of(Widget w)
{
	return w ? static_cast<XawPeer *>(xq_widget_peer(w)) : nullptr;
}

XawPeer *
XawPeer::forWindow(Window win)
{
	return by_window.value(win, nullptr);
}

XawPeer::XawPeer(Widget w, Kind kind, QWidget *parent)
	: QWidget(parent), m_w(w), m_kind(kind)
{
	xq_widget_set_peer(w, this);
	setObjectName(QString::fromLatin1(xq_widget_name(w)));
	setFont(ui_font());
	setMouseTracking(true);
	if (kind == Toggle)
		toggles.insert(this);
	if (kind == Command || kind == Toggle || kind == MenuButton)
		setFocusPolicy(Qt::NoFocus);
	Window win = xq_widget_window(w);
	if (win)
		by_window.insert(win, this);
}

XawPeer::~XawPeer()
{
	toggles.remove(this);
	Window win = xq_widget_window(m_w);
	if (win && by_window.value(win) == this)
		by_window.remove(win);
	if (xq_widget_peer(m_w) == this)
		xq_widget_set_peer(m_w, nullptr);
}

/* a viewport's child: its size from xfig is the minimum, the rest fills */
void
XawPeer::fitToViewport()
{
	XawViewport *vp = qobject_cast<XawViewport *>(of(xq_widget_parent(m_w)));
	if (!vp || m_kind == List)
		return;
	int b = xq_widget_border(m_w);
	bool horiz = xq_res_default(vp->xt(), XtNallowHoriz, False);
	setMinimumSize(horiz ? xq_widget_width(m_w) + 2 * b : 0,
			xq_widget_height(m_w) + 2 * b);
}

Window
XawPeer::createWindow()
{
	Window win = xq_widget_window(m_w);
	if (win)
		return win;
	QSize sz = sizeHint();
	win = xq_create_window(qMax(sz.width(), 1), qMax(sz.height(), 1), devicePixelRatioF());
	xq_widget_set_window(m_w, win);
	by_window.insert(win, this);
	return win;
}

QRect
XawPeer::contentRect() const
{
	int b = xq_widget_border(m_w);
	return rect().adjusted(b, b, -b, -b);
}

static QSize
pixmap_size(Pixmap p)
{
	QImage *img = p ? xq_image(p) : nullptr;
	return img ? img->deviceIndependentSize().toSize() : QSize();
}

QSize
XawPeer::naturalSize() const
{
	int iw = int(res(m_w, XtNinternalWidth, 4));
	int ih = int(res(m_w, XtNinternalHeight, 2));
	switch (m_kind) {
	case Label:
	case Command:
	case Toggle:
	case MenuButton: {
		QSize bm = pixmap_size(Pixmap(res(m_w, XtNbitmap)));
		QSize sz;
		if (bm.isValid()) {
			sz = bm;
		} else {
			QFontMetrics fm(font());
			QStringList lines = qstr(xq_widget_label(m_w)).split('\n');
			int wd = 0;
			for (const QString &l : lines)
				wd = qMax(wd, fm.horizontalAdvance(l));
			sz = QSize(wd, fm.height() * int(lines.size()));
		}
		QSize lb = pixmap_size(Pixmap(res(m_w, XtNleftBitmap)));
		int lm = int(res(m_w, XtNleftMargin, lb.isValid() ? lb.width() + 4 : 0));
		if (lb.isValid())
			lm = qMax(lm, lb.width() + 4);
		/* room for the menu arrow drawn by the peer */
		int arrow = m_kind == MenuButton && !bm.isValid() && !lb.isValid() ? 14 : 0;
		return QSize(sz.width() + 2 * iw + lm + arrow,
				qMax(sz.height(), lb.height()) + 2 * ih);
	}
	default:
		return QSize(qMax(xq_widget_width(m_w), 1), qMax(xq_widget_height(m_w), 1));
	}
}

QSize
XawPeer::sizeHint() const
{
	int b = xq_widget_border(m_w);
	int w = xq_widget_width(m_w), h = xq_widget_height(m_w);
	if (w <= 0 || h <= 0) {
		QSize n = naturalSize();
		if (w <= 0) w = n.width();
		if (h <= 0) h = n.height();
	}
	return QSize(w + 2 * b, h + 2 * b);
}

void
XawPeer::sync()
{
	setEnabled(XtIsSensitive(m_w));
	fitToViewport();
	if (m_adopted)
		scheduleLayout();
	else if (XawPeer *p = of(xq_widget_parent(m_w)))
		p->scheduleLayout();
	update();
}

void
XawPeer::scheduleLayout()
{
	if (m_layoutPending)
		return;
	m_layoutPending = true;
	QTimer::singleShot(0, this, [this] {
		m_layoutPending = false;
		layoutChildren();
	});
}

/* the size xfig asked for with XtNwidth/XtNheight, 0 if none */
static int
requested(Widget w, const char *which)
{
	return int(xq_res_default(w, which, 0));
}

/*
 * Give a widget its layout size: the size xfig requested, or else its
 * natural size.  Forms and Boxes have computed theirs in layoutChildren().
 */
static void
size_widget(Widget w)
{
	XawPeer *p = XawPeer::of(w);
	if (!p)
		return;
	XawPeer::Kind k = p->kind();
	if (k == XawPeer::Form || k == XawPeer::Box || k == XawPeer::Paned)
		return;
	QSize n = p->naturalSize();
	int wd = requested(w, "xqW"), ht = requested(w, "xqH");
	if (wd <= 0)
		wd = n.width();
	if (ht <= 0)
		ht = n.height();
	/*
	 * xfig computed fixed sizes for X11 fonts; Qt's fonts are a little
	 * larger.  Let labels and buttons grow so that their text fits.
	 */
	if ((k == XawPeer::Label || k == XawPeer::Command || k == XawPeer::Toggle ||
			k == XawPeer::MenuButton) && !xq_widget_window(w) &&
			*xq_widget_label(w)) {
		wd = qMax(wd, n.width());
		ht = qMax(ht, n.height());
	}
	xq_widget_set_size(w, wd, ht);
}

/*
 * Where an edge goes when a Form grows by delta (Xaw "chaining"):
 * chained to the top or left it stays, to the bottom or right it moves
 * with the far edge, XtRubber moves proportionally.
 */
static int
chained(int v, int chain, bool horizontal, int delta, int nat, int target)
{
	int nearSide = horizontal ? XtChainLeft : XtChainTop;
	int farSide = horizontal ? XtChainRight : XtChainBottom;
	if (chain == nearSide)
		return v;
	if (chain == farSide)
		return v + delta;
	return nat > 0 ? v * target / nat : v;
}

/* Xaw Form, Box and Paned layouts */
void
XawPeer::layoutChildren()
{
	int n = xq_widget_num_children(m_w);
	int dd = int(res(m_w, XtNdefaultDistance, 4));
	int maxx = 0, maxy = 0;

	if (m_kind == Form) {
		/* first the natural layout, from the constraints */
		for (int i = 0; i < n; ++i) {
			Widget c = xq_widget_child(m_w, i);
			XawPeer *cp = of(c);
			if (!cp || is_shell(c) || xq_widget_is_popup(c))
				continue;
			if (cp->adopted()) {
				size_widget(c);
				continue;
			}
			/* unmanaged children are positioned too: Xaw uses them as
			   anchors for the widgets placed below or beside them */
			bool managed = XtIsManaged(c);
			if (managed && (cp->kind() == Form || cp->kind() == Box)) {
				cp->m_stretch = QSize();
				cp->layoutChildren();
			}
			size_widget(c);
			Widget fh = (Widget)res(c, XtNfromHoriz);
			Widget fv = (Widget)res(c, XtNfromVert);
			int hd = int(res(c, XtNhorizDistance, dd));
			int vd = int(res(c, XtNvertDistance, dd));
			int x = hd, y = vd;
			if (fh)
				x = xq_widget_x(fh) + xq_widget_width(fh) + 2 * xq_widget_border(fh) + hd;
			if (fv)
				y = xq_widget_y(fv) + xq_widget_height(fv) + 2 * xq_widget_border(fv) + vd;
			xq_widget_set_position(c, x, y);
			if (!managed || cp->hostManaged())
				continue;
			int b = xq_widget_border(c);
			maxx = qMax(maxx, x + xq_widget_width(c) + 2 * b);
			maxy = qMax(maxy, y + xq_widget_height(c) + 2 * b);
		}
		/* the Form's size: its contents, at least what xfig asked for
		   (Qt's fonts are wider than the X11 ones xfig sized for), or
		   what the host or the parent Form gives it */
		int natw = maxx + dd, nath = maxy + dd;
		int tw = qMax(natw, requested(m_w, "xqW"));
		int th = qMax(nath, requested(m_w, "xqH"));
		if (m_host) {
			QRect r = contentRect();
			tw = qMax(natw, r.width());
			th = qMax(nath, r.height());
		}
		if (m_stretch.isValid()) {
			tw = qMax(natw, m_stretch.width());
			th = qMax(nath, m_stretch.height());
		}
		xq_widget_set_size(m_w, tw, th);

		/* then move and stretch the children by their chaining */
		int dx = tw - natw, dy = th - nath;
		for (int i = 0; i < n; ++i) {
			Widget c = xq_widget_child(m_w, i);
			XawPeer *cp = of(c);
			if (!cp || is_shell(c) || xq_widget_is_popup(c) || cp->adopted())
				continue;
			if (!XtIsManaged(c)) {
				cp->hide();
				continue;
			}
			if (cp->hostManaged())
				continue;
			int b = xq_widget_border(c);
			int x = xq_widget_x(c), y = xq_widget_y(c);
			int x2 = x + xq_widget_width(c) + 2 * b;
			int y2 = y + xq_widget_height(c) + 2 * b;
			if (dx > 0 || dy > 0) {
				int l = int(res(c, XtNleft, XtRubber)), r = int(res(c, XtNright, XtRubber));
				int t = int(res(c, XtNtop, XtRubber)), bo = int(res(c, XtNbottom, XtRubber));
				int nx = chained(x, l, true, dx, natw, tw);
				int nx2 = chained(x2, r, true, dx, natw, tw);
				int ny = chained(y, t, false, dy, nath, th);
				int ny2 = chained(y2, bo, false, dy, nath, th);
				x = nx;
				y = ny;
				/* a widget does not shrink below its layout size */
				x2 = qMax(nx2, x + xq_widget_width(c) + 2 * b);
				y2 = qMax(ny2, y + xq_widget_height(c) + 2 * b);
				int nw = x2 - x - 2 * b, nh = y2 - y - 2 * b;
				xq_widget_set_position(c, x, y);
				if (nw != xq_widget_width(c) || nh != xq_widget_height(c)) {
					xq_widget_set_size(c, nw, nh);
					if (cp->kind() == Form || cp->kind() == Box) {
						cp->m_stretch = QSize(nw, nh);
						cp->layoutChildren();
					}
				}
			}
			cp->setGeometry(x, y, x2 - x, y2 - y);
			cp->show();
		}
	} else if (m_kind == Box || m_kind == Paned) {
		bool vertical = m_kind == Paned ||
			res(m_w, XtNorientation, XtorientHorizontal) == XtorientVertical;
		int hs = int(res(m_w, XtNhSpace, m_kind == Paned ? 0 : 4));
		int vs = int(res(m_w, XtNvSpace, m_kind == Paned ? 0 : 4));
		/* in a viewport, a Box does not wrap: it scrolls */
		bool in_viewport = qobject_cast<XawViewport *>(of(xq_widget_parent(m_w)));
		int limit = in_viewport ? 0 : requested(m_w, "xqW");
		int x = hs, y = vs, rowh = 0;
		for (int i = 0; i < n; ++i) {
			Widget c = xq_widget_child(m_w, i);
			XawPeer *cp = of(c);
			if (!cp || is_shell(c) || xq_widget_is_popup(c) || cp->adopted())
				continue;
			if (!XtIsManaged(c)) {
				cp->hide();
				continue;
			}
			if (cp->kind() == Form || cp->kind() == Box)
				cp->layoutChildren();
			size_widget(c);
			int b = xq_widget_border(c);
			int cw = xq_widget_width(c) + 2 * b, ch = xq_widget_height(c) + 2 * b;
			if (vertical) {
				if (i > 0)
					y += vs;
				x = hs;
			} else if (limit > 0 && x + cw > limit && x > hs) {
				x = hs;
				y += rowh + vs;
				rowh = 0;
			}
			xq_widget_set_position(c, x, y);
			cp->setGeometry(x, y, cw, ch);
			cp->show();
			maxx = qMax(maxx, x + cw);
			maxy = qMax(maxy, y + ch);
			if (vertical)
				y += ch;
			else {
				x += cw + hs;
				rowh = qMax(rowh, ch);
			}
		}
		int bw = !in_viewport && requested(m_w, "xqW") > 0 ? requested(m_w, "xqW") : maxx + hs;
		/* in a viewport, a Box is as tall as its contents: it scrolls */
		int bh = qMax(maxy + vs, in_viewport ? 0 : requested(m_w, "xqH"));
		if (m_stretch.isValid())
			bh = qMax(bh, m_stretch.height());
		xq_widget_set_size(m_w, qMax(bw, maxx + hs), bh);
	}
	if (!m_host && m_kind != Shell) {
		int b = xq_widget_border(m_w);
		resize(xq_widget_width(m_w) + 2 * b, xq_widget_height(m_w) + 2 * b);
	}
	fitToViewport();
	updateGeometry();
	if (XawViewport *vp = qobject_cast<XawViewport *>(of(xq_widget_parent(m_w))))
		vp->childLaidOut(this);
	if (XawShell *sh = qobject_cast<XawShell *>(window()))
		if (sh != this && sh->isVisible())
			sh->fitToChild();
}

/* ------------------------------------------------------------ painting */

static QColor
pixel_color(XtArgVal p)
{
	return QColor::fromRgb(QRgb(0xff000000u | (unsigned long)p));
}

static void
draw_pixmap(QPainter &p, Pixmap pm, QPoint at, QColor fg)
{
	QImage *img = xq_image(pm);
	if (!img)
		return;
	if (xq_drawable_depth(pm) == 1) {
		/* a bitmap: set bits in the foreground color */
		QImage mask = img->createMaskFromColor(0xffffffffu, Qt::MaskOutColor);
		QBitmap bm = QBitmap::fromImage(mask);
		QPixmap colored(img->size());
		colored.fill(Qt::transparent);
		QPainter cp(&colored);
		cp.setPen(fg);
		cp.setBackgroundMode(Qt::TransparentMode);
		cp.drawPixmap(0, 0, bm);
		cp.end();
		p.drawPixmap(at, colored);
	} else {
		p.drawImage(at, *img);
	}
}

void
XawPeer::paintFace(QPainter &p, QRect r)
{
	XtArgVal v;
	bool button = m_kind == Command || m_kind == Toggle || m_kind == MenuButton;
	bool on = m_kind == Toggle && res(m_w, XtNstate, False);
	QColor bg = palette().color(QPalette::Window);
	if (xq_res(m_w, XtNbackground, &v))
		bg = pixel_color(v);
	else if (button)
		bg = palette().color(QPalette::Button);
	if (button && isEnabled() && (m_down || on))
		bg = bg.darker(m_down ? 125 : 115);
	else if (button && isEnabled() && m_hover)
		bg = bg.lighter(104);
	p.fillRect(r, bg);
	Pixmap bgpm = Pixmap(res(m_w, XtNbackgroundPixmap));
	if (bgpm && xq_image(bgpm))
		p.drawTiledPixmap(r, QPixmap::fromImage(*xq_image(bgpm)));
}

void
XawPeer::paintLabel(QPainter &p, QRect r)
{
	XtArgVal v;
	QColor fg = xq_res(m_w, XtNforeground, &v) ? pixel_color(v) :
		palette().color(QPalette::ButtonText);
	if (!isEnabled())
		fg = palette().color(QPalette::Disabled, QPalette::ButtonText);
	int iw = int(res(m_w, XtNinternalWidth, 4));
	int x = r.left() + iw;

	Pixmap lb = Pixmap(res(m_w, XtNleftBitmap));
	if (lb) {
		QSize s = pixmap_size(lb);
		draw_pixmap(p, lb, QPoint(x, r.center().y() - s.height() / 2), fg);
		x += int(res(m_w, XtNleftMargin, s.width() + 4)) - 0;
	}
	Pixmap bm = Pixmap(res(m_w, XtNbitmap));
	if (bm && xq_image(bm)) {
		QSize s = pixmap_size(bm);
		draw_pixmap(p, bm, QPoint(r.center().x() - s.width() / 2 + 1,
				r.center().y() - s.height() / 2 + 1), fg);
		return;
	}
	QString text = qstr(xq_widget_label(m_w));
	if (text.isEmpty())
		return;
	int just = int(res(m_w, XtNjustify, XtJustifyCenter));
	int align = Qt::AlignVCenter;
	if (lb || just == XtJustifyLeft)
		align |= Qt::AlignLeft;
	else if (just == XtJustifyRight)
		align |= Qt::AlignRight;
	else
		align |= Qt::AlignHCenter;
	p.setPen(fg);
	p.setFont(font());
	QRect tr = r.adjusted(x - r.left(), 0, -iw, 0);
	if (!(align & Qt::AlignLeft))
		tr = r.adjusted(iw, 0, -iw, 0);
	/* a single line that does not fit is shortened, the tooltip has it all */
	if (!text.contains('\n') && p.fontMetrics().horizontalAdvance(text) > tr.width()) {
		QString full = text;
		text = p.fontMetrics().elidedText(text.trimmed(), Qt::ElideRight, tr.width());
		if (toolTip() != full.trimmed())
			const_cast<XawPeer *>(this)->setToolTip(full.trimmed());
	}
	p.drawText(tr, align, text);
	if (m_kind == MenuButton && !bm && !lb) {
		/* a small arrow marks a menu */
		int ax = r.right() - 7, ay = r.center().y();
		QPolygon tri;
		tri << QPoint(ax - 3, ay - 1) << QPoint(ax + 3, ay - 1) << QPoint(ax, ay + 2);
		p.setBrush(fg);
		p.setPen(Qt::NoPen);
		p.drawPolygon(tri);
	}
}

void
XawPeer::paintEvent(QPaintEvent *e)
{
	QPainter p(this);
	QRect r = contentRect();
	Window win = xq_widget_window(m_w);
	QImage *img = win ? xq_image(win) : nullptr;
	if (img && (m_drawn || m_host)) {
		/* xfig draws this window: show what it drew */
		QRect er = e->rect().intersected(r);
		qreal dpr = img->devicePixelRatio();
		p.drawImage(er, *img, QRectF((er.topLeft() - r.topLeft()) * dpr,
				QSizeF(er.size()) * dpr));
	} else if (m_kind != Form && m_kind != Box && m_kind != Paned &&
			m_kind != Shell && m_kind != Generic) {
		paintFace(p, r);
		if (m_kind != Text && m_kind != List && m_kind != Scrollbar &&
				m_kind != Viewport)
			paintLabel(p, r);
	} else {
		paintFace(p, r);
	}
	int b = xq_widget_border(m_w);
	if (b > 0) {
		bool button = m_kind == Command || m_kind == Toggle ||
			m_kind == MenuButton;
		QColor bc = palette().color(QPalette::Mid);
		if (button && isEnabled())
			bc = palette().color(QPalette::Dark);
		p.setPen(QPen(bc, 1));
		p.setBrush(Qt::NoBrush);
		p.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
	}
}

void
XawPeer::resizeEvent(QResizeEvent *)
{
	Window win = xq_widget_window(m_w);
	QRect r = contentRect();
	if (m_host)
		xq_widget_set_size(m_w, r.width(), r.height());
	if (win) {
		xq_resize_window(win, qMax(r.width(), 1), qMax(r.height(), 1),
				devicePixelRatioF());
		XClearWindow(xq_display, win);
	}
	if (onResize)
		onResize(r.width(), r.height());
	/* a Form sized by the host lays out its children again */
	if (m_host && (m_kind == Form || m_kind == Box))
		layoutChildren();
	if (win) {
		XEvent ev;
		memset(&ev, 0, sizeof ev);
		ev.xexpose.type = Expose;
		ev.xexpose.display = xq_display;
		ev.xexpose.window = win;
		ev.xexpose.width = r.width();
		ev.xexpose.height = r.height();
		sendEvent(ev);
	}
}

/* -------------------------------------------------------------- events */

int
XawPeer::sendEvent(XEvent &ev)
{
	return xq_widget_event(m_w, &ev);
}

/*
 * X modifier state.  On macOS Qt reports Command as ControlModifier and
 * Control as MetaModifier: the Control key acts as X Control, Option as
 * X Meta (Mod1).  Command is reserved for menu shortcuts.
 */
unsigned
XawPeer::xState(Qt::KeyboardModifiers m, Qt::MouseButtons b)
{
	unsigned s = 0;
	if (m & Qt::ShiftModifier)
		s |= ShiftMask;
#ifdef Q_OS_MACOS
	if (m & Qt::MetaModifier)
		s |= ControlMask;
#else
	if (m & Qt::ControlModifier)
		s |= ControlMask;
#endif
	if (m & Qt::AltModifier)
		s |= Mod1Mask;
	if (b & Qt::LeftButton)
		s |= Button1Mask;
	if (b & Qt::MiddleButton)
		s |= Button2Mask;
	if (b & Qt::RightButton)
		s |= Button3Mask;
	return s;
}

/* xfig needs three buttons: without a middle button, Command-click is it */
unsigned
XawPeer::xButton(QMouseEvent *e) const
{
	switch (e->button()) {
	case Qt::LeftButton:
#ifdef Q_OS_MACOS
		if (e->modifiers() & Qt::ControlModifier)
			return Button2;
#endif
		return Button1;
	case Qt::MiddleButton:	return Button2;
	case Qt::RightButton:	return Button3;
	default:		return 0;
	}
}

void
XawPeer::makeButton(XEvent &ev, int type, unsigned button, QPointF pos,
		Qt::KeyboardModifiers mods, Qt::MouseButtons buttons)
{
	memset(&ev, 0, sizeof ev);
	XButtonEvent &b = ev.xbutton;
	QPointF c = pos - QPointF(contentRect().topLeft());
	b.type = type;
	b.display = xq_display;
	b.window = xq_widget_window(m_w);
	b.x = int(std::floor(c.x()));
	b.y = int(std::floor(c.y()));
	QPoint g = mapToGlobal(pos.toPoint());
	b.x_root = g.x();
	b.y_root = g.y();
	b.button = button;
	b.state = xState(mods, buttons);
	/* X reports the state before the event */
	unsigned bmask = button >= 1 && button <= 5 ? Button1Mask << (button - 1) : 0;
	if (type == ButtonPress)
		b.state &= ~bmask;
	else
		b.state |= bmask;
	b.same_screen = True;
	b.time = Time(QDateTime::currentMSecsSinceEpoch());
	xq_set_pointer(b.window, b.x, b.y, b.state);
}

void
XawPeer::mousePressEvent(QMouseEvent *e)
{
	if (focusPolicy() != Qt::NoFocus)
		setFocus(Qt::MouseFocusReason);
	m_pressed = xButton(e);
	if (!m_pressed)
		return;
	m_down = (m_kind == Command || m_kind == Toggle) && m_pressed == Button1;
	update();
	XEvent ev;
	makeButton(ev, ButtonPress, m_pressed, e->position(), e->modifiers(), e->buttons());
	xq_click_count = 1;
	bool translated = xq_translation_matches(m_w, &ev);
	sendEvent(ev);
	if (m_kind == MenuButton && m_pressed == Button1 && !translated) {
		/* pop up the menu named by menuName below the button */
		const char *name = (const char *)res(m_w, XtNmenuName);
		Widget menu = nullptr;
		for (Widget a = m_w; a && !menu && name; a = xq_widget_parent(a)) {
			for (int i = 0; i < xq_widget_num_children(a); ++i)
				if (!strcmp(xq_widget_name(xq_widget_child(a, i)), name)) {
					menu = xq_widget_child(a, i);
					break;
				}
		}
		if (menu) {
			QPoint g = mapToGlobal(rect().bottomLeft());
			xq_widget_set_position(menu, g.x(), g.y());
			m_pressed = 0;
			m_down = false;
			XtPopupSpringLoaded(menu);
		}
	}
}

void
XawPeer::mouseReleaseEvent(QMouseEvent *e)
{
	unsigned button = m_pressed ? m_pressed : xButton(e);
	m_pressed = 0;
	bool wasdown = m_down;
	m_down = false;
	update();
	if (!button)
		return;
	XEvent ev;
	makeButton(ev, ButtonRelease, button, e->position(), e->modifiers(), e->buttons());
	xq_click_count = 1;
	bool translated = xq_translation_matches(m_w, &ev);
	QPointer<XawPeer> self(this);
	sendEvent(ev);
	if (self && wasdown && !translated && button == Button1 &&
			rect().contains(e->position().toPoint()))
		activate(e);
}

void
XawPeer::mouseDoubleClickEvent(QMouseEvent *e)
{
	/* X sees a second press; the release carries the click count */
	mousePressEvent(e);
	unsigned button = m_pressed;
	if (!button)
		return;
	XEvent ev;
	makeButton(ev, ButtonRelease, button, e->position(), e->modifiers(),
			e->buttons() | e->button());
	xq_click_count = 2;
	if (xq_translation_matches(m_w, &ev)) {
		m_pressed = 0;
		m_down = false;
		sendEvent(ev);
	}
	xq_click_count = 1;
}

/* the default action of Command (notify) and Toggle (toggle, notify) */
void
XawPeer::activate(QMouseEvent *)
{
	if (m_kind == Command) {
		xq_call_callbacks(m_w, XtNcallback, nullptr);
	} else if (m_kind == Toggle) {
		Boolean on = !res(m_w, XtNstate, False);
		Widget group = (Widget)res(m_w, XtNradioGroup);
		if (on && group) {
			/* unset the other members of the radio group */
			for (XawPeer *t : toggles) {
				if (t == this || !res(t->xt(), XtNstate, False))
					continue;
				Widget g = (Widget)res(t->xt(), XtNradioGroup);
				if (g == m_w || g == group || t->xt() == group) {
					XtVaSetValues(t->xt(), XtNstate, False, NULL);
					xq_call_callbacks(t->xt(), XtNcallback, (XtPointer)0);
				}
			}
		}
		XtVaSetValues(m_w, XtNstate, (XtArgVal)on, NULL);
		xq_call_callbacks(m_w, XtNcallback, (XtPointer)(intptr_t)on);
	}
}

void
XawPeer::mouseMoveEvent(QMouseEvent *e)
{
	XEvent ev;
	memset(&ev, 0, sizeof ev);
	XMotionEvent &m = ev.xmotion;
	QPointF c = e->position() - QPointF(contentRect().topLeft());
	m.type = MotionNotify;
	m.display = xq_display;
	m.window = xq_widget_window(m_w);
	m.x = int(std::floor(c.x()));
	m.y = int(std::floor(c.y()));
	QPoint g = e->globalPosition().toPoint();
	m.x_root = g.x();
	m.y_root = g.y();
	m.state = xState(e->modifiers(), e->buttons());
	/* an emulated middle button drags as button 2 */
	if (m_pressed == Button2 && (m.state & Button1Mask))
		m.state = (m.state & ~Button1Mask) | Button2Mask;
	m.same_screen = True;
	m.time = Time(QDateTime::currentMSecsSinceEpoch());
	xq_set_pointer(m.window, m.x, m.y, m.state);
	sendEvent(ev);
}

/* the wheel is buttons 4 (up) and 5 (down), 6 and 7 horizontally */
void
XawPeer::wheelEvent(QWheelEvent *e)
{
	m_wheel += QPointF(e->angleDelta());
	while (std::abs(m_wheel.y()) >= 120 || std::abs(m_wheel.x()) >= 120) {
		unsigned button;
		if (std::abs(m_wheel.y()) >= 120) {
			button = m_wheel.y() > 0 ? Button4 : Button5;
			m_wheel.ry() -= m_wheel.y() > 0 ? 120 : -120;
		} else {
			button = m_wheel.x() > 0 ? Button6 : Button7;
			m_wheel.rx() -= m_wheel.x() > 0 ? 120 : -120;
		}
		XEvent ev;
		makeButton(ev, ButtonPress, button, e->position(), e->modifiers(), e->buttons());
		sendEvent(ev);
		makeButton(ev, ButtonRelease, button, e->position(), e->modifiers(), e->buttons());
		sendEvent(ev);
	}
	e->accept();
}

static KeySym
keysym_for(QKeyEvent *e)
{
	switch (e->key()) {
	case Qt::Key_Backspace:	return XK_BackSpace;
	case Qt::Key_Tab:	return XK_Tab;
	case Qt::Key_Backtab:	return XK_Tab;
	case Qt::Key_Return:	return XK_Return;
	case Qt::Key_Enter:	return XK_KP_Enter;
	case Qt::Key_Escape:	return XK_Escape;
	case Qt::Key_Delete:	return XK_Delete;
	case Qt::Key_Home:	return XK_Home;
	case Qt::Key_End:	return XK_End;
	case Qt::Key_Left:	return XK_Left;
	case Qt::Key_Up:	return XK_Up;
	case Qt::Key_Right:	return XK_Right;
	case Qt::Key_Down:	return XK_Down;
	case Qt::Key_PageUp:	return XK_Prior;
	case Qt::Key_PageDown:	return XK_Next;
	case Qt::Key_Shift:	return XK_Shift_L;
	case Qt::Key_Control:	return XK_Control_L;
	case Qt::Key_Meta:	return XK_Control_L;	/* macOS Control */
	case Qt::Key_Alt:	return XK_Alt_L;
	case Qt::Key_CapsLock:	return XK_Caps_Lock;
	case Qt::Key_Space:	return XK_space;
	default:
		break;
	}
	if (e->key() >= Qt::Key_F1 && e->key() <= Qt::Key_F35)
		return XK_F1 + (e->key() - Qt::Key_F1);
	QString t = e->text();
	if (!t.isEmpty() && t.at(0).isPrint()) {
		uint c = t.toUcs4().value(0);
		if (c >= 0x20 && c < 0x100)
			return c;
		if (c >= 0x100)
			return 0x01000000 + c;
	}
	/* a letter with Control or Option: use the key itself */
	if (e->key() >= Qt::Key_A && e->key() <= Qt::Key_Z)
		return (e->modifiers() & Qt::ShiftModifier) ?
			e->key() : e->key() - Qt::Key_A + 'a';
	if (e->key() >= 0x20 && e->key() < 0x7f)
		return e->key();
	return 0;
}

bool
XawPeer::makeKey(XEvent &ev, int type, QKeyEvent *e)
{
	memset(&ev, 0, sizeof ev);
	XKeyEvent &k = ev.xkey;
	k.type = type;
	k.display = xq_display;
	k.window = xq_widget_window(m_w);
	QPointF pos = mapFromGlobal(QCursor::pos()) - QPointF(contentRect().topLeft());
	k.x = int(pos.x());
	k.y = int(pos.y());
	k.state = xState(e->modifiers(), QGuiApplication::mouseButtons());
	k.keycode = e->nativeScanCode();
	k.same_screen = True;
	k.time = Time(QDateTime::currentMSecsSinceEpoch());
	KeySym sym = keysym_for(e);
	QByteArray text;
	/* control characters, as XLookupString makes them */
	if ((k.state & ControlMask) && sym >= 'a' && sym <= 'z')
		text = QByteArray(1, char(sym - 'a' + 1));
	else if (sym == XK_BackSpace)
		text = QByteArray(1, '\b');
	else if (sym == XK_Tab)
		text = QByteArray(1, '\t');
	else if (sym == XK_Return || sym == XK_KP_Enter)
		text = QByteArray(1, '\r');
	else if (sym == XK_Escape)
		text = QByteArray(1, '\033');
	else if (sym == XK_Delete)
		text = QByteArray(1, '\177');
	else if (!(k.state & Mod1Mask) && !e->text().isEmpty() && e->text().at(0).isPrint())
		text = e->text().toUtf8();
	xq_set_key(text.constData(), sym);
	return sym != 0 || !text.isEmpty();
}

void
XawPeer::keyPressEvent(QKeyEvent *e)
{
	XEvent ev;
	if (makeKey(ev, KeyPress, e))
		sendEvent(ev);
}

void
XawPeer::keyReleaseEvent(QKeyEvent *e)
{
	XEvent ev;
	if (!e->isAutoRepeat() && makeKey(ev, KeyRelease, e))
		sendEvent(ev);
}

bool
XawPeer::focusNextPrevChild(bool next)
{
	/* the canvas uses Tab itself */
	if (m_host)
		return false;
	return QWidget::focusNextPrevChild(next);
}

void
XawPeer::enterEvent(QEnterEvent *e)
{
	m_hover = true;
	update();
	XEvent ev;
	memset(&ev, 0, sizeof ev);
	ev.xcrossing.type = EnterNotify;
	ev.xcrossing.display = xq_display;
	ev.xcrossing.window = xq_widget_window(m_w);
	ev.xcrossing.x = int(e->position().x());
	ev.xcrossing.y = int(e->position().y());
	ev.xcrossing.state = xState(QGuiApplication::keyboardModifiers(),
			QGuiApplication::mouseButtons());
	sendEvent(ev);
}

void
XawPeer::leaveEvent(QEvent *)
{
	m_hover = false;
	update();
	XEvent ev;
	memset(&ev, 0, sizeof ev);
	ev.xcrossing.type = LeaveNotify;
	ev.xcrossing.display = xq_display;
	ev.xcrossing.window = xq_widget_window(m_w);
	ev.xcrossing.state = xState(QGuiApplication::keyboardModifiers(),
			QGuiApplication::mouseButtons());
	sendEvent(ev);
}

/* ---------------------------------------------------------------- shell */

XawShell::XawShell(Widget w)
	: XawPeer(w, Shell, nullptr)
{
	const char *c = xq_widget_class(w);
	if (!strcmp(c, "OverrideShell"))
		setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
	else
		setWindowFlags(Qt::Dialog);
	setAttribute(Qt::WA_DeleteOnClose, false);
	setFocusPolicy(Qt::StrongFocus);
}

void
XawShell::fitToChild()
{
	if (m_fitting)
		return;
	m_fitting = true;
	QSize sz;
	for (int i = 0; i < xq_widget_num_children(m_w); ++i) {
		Widget c = xq_widget_child(m_w, i);
		XawPeer *cp = of(c);
		if (!cp || xq_widget_is_popup(c) || is_shell(c))
			continue;
		if (cp->kind() == Form || cp->kind() == Box || cp->kind() == Paned)
			cp->layoutChildren();
		size_widget(c);
		int b = xq_widget_border(c);
		cp->setGeometry(0, 0, xq_widget_width(c) + 2 * b, xq_widget_height(c) + 2 * b);
		cp->show();
		sz = sz.expandedTo(cp->size());
	}
	if (sz.isValid() && !sz.isEmpty()) {
		setFixedSize(sz);
		xq_widget_set_size(m_w, sz.width(), sz.height());
	}
	m_fitting = false;
}

void
XawShell::popup(XtGrabKind grab)
{
	const char *title = (const char *)res(m_w, XtNtitle);
	if (title)
		setWindowTitle(qstr(title));
	fitToChild();
	if (grab == XtGrabExclusive && windowType() != Qt::ToolTip)
		setWindowModality(Qt::ApplicationModal);
	else
		setWindowModality(Qt::NonModal);
	/* xfig places its dialogs with XtNx/XtNy, in screen coordinates */
	QPoint pos(xq_widget_x(m_w), xq_widget_y(m_w));
	if (pos.isNull() && QApplication::activeWindow())
		pos = QApplication::activeWindow()->geometry().center() -
			QPoint(width() / 2, height() / 2);
	move(pos);
	show();
	raise();
	if (windowType() != Qt::ToolTip)
		activateWindow();
}

void
XawShell::popdown()
{
	hide();
}

void
XawShell::closeEvent(QCloseEvent *e)
{
	/* ask xfig, as a window manager would with WM_DELETE_WINDOW */
	e->ignore();
	XEvent ev;
	memset(&ev, 0, sizeof ev);
	ev.xclient.type = ClientMessage;
	ev.xclient.display = xq_display;
	ev.xclient.window = xq_widget_window(m_w);
	ev.xclient.format = 32;
	if (!sendEvent(ev)) {
		/* no WM_PROTOCOLS translation: just pop down */
		XtPopdown(m_w);
	}
}

/* ----------------------------------------------------------- menus */

static void
popup_simple_menu(Widget menu)
{
	QMenu qm;
	qm.setFont(ui_font());
	for (int i = 0; i < xq_widget_num_children(menu); ++i) {
		Widget e = xq_widget_child(menu, i);
		if (!XtIsManaged(e))
			continue;
		if (is(e, "SmeLine")) {
			qm.addSeparator();
			continue;
		}
		QString label = qstr(xq_widget_label(e));
		QAction *a = qm.addAction(label);
		a->setEnabled(XtIsSensitive(e));
		Pixmap lb = Pixmap(res(e, XtNleftBitmap));
		if (lb && xq_image(lb) && !xq_image(lb)->isNull()) {
			/* xfig marks checked entries with a left bitmap */
			QImage *img = xq_image(lb);
			bool blank = true;
			for (int y = 0; y < img->height() && blank; ++y)
				for (int x = 0; x < img->width(); ++x)
					if ((img->pixel(x, y) & 0xffffff) != 0) {
						blank = false;
						break;
					}
			if (!blank) {
				a->setCheckable(true);
				a->setChecked(true);
			}
		}
		a->setData(QVariant::fromValue((void *)e));
	}
	QPoint at(xq_widget_x(menu), xq_widget_y(menu));
	if (at.isNull())
		at = QCursor::pos();
	QAction *chosen = qm.exec(at);
	XtPopdown(menu);
	if (chosen) {
		Widget e = (Widget)chosen->data().value<void *>();
		xq_call_callbacks(e, XtNcallback, nullptr);
	}
}

/* ------------------------------------------------------------- text */

class XawLineEdit : public QLineEdit
{
public:
	using QLineEdit::QLineEdit;
};

XawText::XawText(Widget w, QWidget *parent)
	: XawPeer(w, Text, parent)
{
	bool multi = res(w, XtNscrollVertical, XawtextScrollNever) != XawtextScrollNever ||
		xq_widget_height(w) > 2 * QFontMetrics(ui_font()).height() + 8;
	if (multi) {
		m_multi = new QPlainTextEdit(this);
		m_multi->setFont(ui_font());
		m_multi->setFrameShape(QFrame::NoFrame);
		m_multi->installEventFilter(this);
		connect(m_multi, &QPlainTextEdit::textChanged, this, [this] {
			if (!m_syncing)
				xq_call_callbacks(m_w, XtNcallback, nullptr);
		});
	} else {
		m_line = new QLineEdit(this);
		m_line->setFont(ui_font());
		m_line->setFrame(false);
		m_line->setTextMargins(2, 0, 2, 0);
		m_line->installEventFilter(this);
		connect(m_line, &QLineEdit::textEdited, this, [this] {
			/* the text source's callback: the text changed */
			xq_call_callbacks(m_w, XtNcallback, nullptr);
		});
	}
	setFocusProxy(m_line ? static_cast<QWidget *>(m_line) : m_multi);
	sync();
}

QSize
XawText::naturalSize() const
{
	QFontMetrics fm(font());
	return QSize(100, fm.height() + 6);
}

QString
XawText::current() const
{
	return m_line ? m_line->text() : m_multi->toPlainText();
}

void
XawText::setCurrent(const QString &s)
{
	m_syncing = true;
	if (m_line) {
		m_line->setText(s);
		m_line->setCursorPosition(int(s.size()));
	} else {
		m_multi->setPlainText(s);
	}
	m_syncing = false;
}

void
XawText::sync()
{
	XtArgVal v;
	if (xq_res(m_w, XtNstring, &v)) {
		QString s = qstr((const char *)v);
		if (s != current())
			setCurrent(s);
	}
	bool ro = res(m_w, XtNeditType, XawtextRead) != XawtextEdit &&
		res(m_w, XtNeditType, XawtextRead) != XawtextAppend;
	if (m_line)
		m_line->setReadOnly(ro);
	else
		m_multi->setReadOnly(ro);
	XawPeer::sync();
}

const char *
XawText::text()
{
	m_utf8 = current().toUtf8();
	return m_utf8.constData();
}

void
XawText::replace(long from, long to, const QString &t)
{
	QString s = current();
	from = qBound(0L, from, long(s.size()));
	to = qBound(from, to, long(s.size()));
	s.replace(int(from), int(to - from), t);
	setCurrent(s);
	setInsertion(from + t.size());
}

long
XawText::insertion() const
{
	return m_line ? m_line->cursorPosition() : m_multi->textCursor().position();
}

void
XawText::setInsertion(long pos)
{
	if (m_line) {
		m_line->setCursorPosition(int(pos));
	} else {
		QTextCursor c = m_multi->textCursor();
		c.setPosition(int(qBound(0L, pos, long(current().size()))));
		m_multi->setTextCursor(c);
	}
}

void
XawText::resizeEvent(QResizeEvent *e)
{
	QWidget *edit = m_line ? static_cast<QWidget *>(m_line) : m_multi;
	edit->setGeometry(contentRect());
	XawPeer::resizeEvent(e);
}

/* keys bound in the translations (Return, Escape, ...) go to xfig */
bool
XawText::eventFilter(QObject *o, QEvent *e)
{
	if (e->type() == qtev::KeyPressEvent) {
		QKeyEvent *k = static_cast<QKeyEvent *>(e);
		XEvent ev;
		if (makeKey(ev, KeyPress, k) && xq_translation_matches(m_w, &ev)) {
			sendEvent(ev);
			return true;
		}
	} else if (e->type() == QEvent::FocusIn) {
		XEvent ev;
		memset(&ev, 0, sizeof ev);
		ev.xcrossing.type = EnterNotify;
		ev.xcrossing.display = xq_display;
		sendEvent(ev);
	}
	return QObject::eventFilter(o, e);
}

/* ------------------------------------------------------------- list */

XawList::XawList(Widget w, QWidget *parent)
	: XawPeer(w, List, parent)
{
	m_list = new QListWidget(this);
	m_list->setFont(ui_font());
	m_list->setFrameShape(QFrame::NoFrame);
	m_list->setSelectionMode(QAbstractItemView::SingleSelection);
	m_list->setUniformItemSizes(true);
	m_list->viewport()->installEventFilter(this);
	m_list->installEventFilter(this);
	setFocusProxy(m_list);
	connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) {
		notify(m_list->row(it));
	});
	String *list = (String *)res(w, XtNlist);
	setStrings(list, int(res(w, XtNnumberStrings, 0)));
}

QSize
XawList::naturalSize() const
{
	return QSize(qMax(xq_widget_width(m_w), 100), qMax(xq_widget_height(m_w), 60));
}

void
XawList::setStrings(String *list, int n)
{
	m_list->clear();
	if (!list)
		return;
	if (n <= 0)
		for (n = 0; list[n]; ++n)
			;
	for (int i = 0; i < n; ++i)
		m_list->addItem(qstr(list[i]));
}

void
XawList::highlight(int i)
{
	if (i < 0)
		m_list->clearSelection();
	else
		m_list->setCurrentRow(i);
}

int
XawList::current() const
{
	QListWidgetItem *it = m_list->currentItem();
	return it && it->isSelected() ? m_list->row(it) : XAW_LIST_NONE;
}

void
XawList::notify(int row)
{
	XawListReturnStruct r;
	QByteArray s = m_list->item(row) ? m_list->item(row)->text().toUtf8() : QByteArray();
	r.string = s.data();
	r.list_index = row;
	xq_call_callbacks(m_w, XtNcallback, &r);
}

bool
XawList::eventFilter(QObject *o, QEvent *e)
{
	if (e->type() == QEvent::MouseButtonDblClick) {
		QMouseEvent *m = static_cast<QMouseEvent *>(e);
		QListWidgetItem *it = m_list->itemAt(m->position().toPoint());
		if (it) {
			m_list->setCurrentItem(it);
			notify(m_list->row(it));
			XEvent ev;
			makeButton(ev, ButtonRelease, Button1, m->position(),
					m->modifiers(), m->buttons() | Qt::LeftButton);
			xq_click_count = 2;
			sendEvent(ev);
			xq_click_count = 1;
		}
		return true;
	}
	if (e->type() == QEvent::MouseButtonRelease) {
		QMouseEvent *m = static_cast<QMouseEvent *>(e);
		if (m->button() == Qt::RightButton) {
			XEvent ev;
			makeButton(ev, ButtonRelease, Button3, m->position(),
					m->modifiers(), m->buttons());
			sendEvent(ev);
		}
	}
	if (e->type() == qtev::KeyPressEvent) {
		QKeyEvent *k = static_cast<QKeyEvent *>(e);
		XEvent ev;
		if (makeKey(ev, KeyPress, k) && xq_translation_matches(m_w, &ev)) {
			sendEvent(ev);
			return true;
		}
	}
	return QObject::eventFilter(o, e);
}

void
XawList::resizeEvent(QResizeEvent *e)
{
	m_list->setGeometry(contentRect());
	XawPeer::resizeEvent(e);
}

/* -------------------------------------------------------- scrollbar */

XawScrollbar::XawScrollbar(Widget w, QWidget *parent)
	: XawPeer(w, Scrollbar, parent)
{
	bool vertical = res(w, XtNorientation, XtorientVertical) == XtorientVertical;
	m_bar = new QScrollBar(vertical ? Qt::Vertical : Qt::Horizontal, this);
	m_bar->setRange(0, 10000);
	m_bar->setPageStep(1000);
	connect(m_bar, &QScrollBar::valueChanged, this, [this](int v) {
		if (m_setting)
			return;
		float top = float(v) / float(m_bar->maximum() + m_bar->pageStep());
		xq_call_callbacks(m_w, XtNjumpProc, &top);
	});
}

QSize
XawScrollbar::naturalSize() const
{
	bool vertical = m_bar->orientation() == Qt::Vertical;
	int t = int(res(m_w, XtNthickness, 14));
	int l = vertical ? xq_widget_height(m_w) : xq_widget_width(m_w);
	return vertical ? QSize(t, qMax(l, 20)) : QSize(qMax(l, 20), t);
}

void
XawScrollbar::setThumb(float top, float shown)
{
	m_setting = true;
	if (shown > 0 && shown <= 1) {
		int page = qMax(1, int(shown * 10000));
		m_bar->setPageStep(page);
		m_bar->setMaximum(qMax(0, 10000 - page));
	}
	if (top >= 0 && top <= 1)
		m_bar->setValue(int(top * 10000));
	m_setting = false;
}

void
XawScrollbar::resizeEvent(QResizeEvent *e)
{
	m_bar->setGeometry(contentRect());
	XawPeer::resizeEvent(e);
}

/* --------------------------------------------------------- viewport */

XawViewport::XawViewport(Widget w, QWidget *parent)
	: XawPeer(w, Viewport, parent)
{
	m_area = new QScrollArea(this);
	m_area->setFrameShape(QFrame::NoFrame);
	m_area->setWidgetResizable(false);
	m_area->setHorizontalScrollBarPolicy(res(w, XtNallowHoriz, False) ?
			Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
	m_area->setVerticalScrollBarPolicy(res(w, XtNallowVert, False) ?
			Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
	if (res(w, XtNforceBars, False)) {
		if (res(w, XtNallowVert, False))
			m_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
		if (res(w, XtNallowHoriz, False))
			m_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	}
}

QWidget *
XawViewport::childArea() const
{
	return m_area->viewport();
}

void
XawViewport::setChild(XawPeer *child)
{
	m_area->setWidget(child);
	/* the child fills the viewport, but is at least as large as xfig
	   makes it (see fitToViewport) */
	m_area->setWidgetResizable(true);
	child->setHostManaged(false);
	child->setAdopted(true);	/* placed by the scroll area */
	child->show();
	child->scheduleLayout();
}

void
XawViewport::childLaidOut(XawPeer *child)
{
	if (!m_fit)
		return;
	int h = child->minimumHeight() + 2 * xq_widget_border(m_w);
	if (m_area->horizontalScrollBarPolicy() != Qt::ScrollBarAlwaysOff)
		h += m_area->horizontalScrollBar()->sizeHint().height();
	setFixedHeight(h);
}

QSize
XawViewport::naturalSize() const
{
	return QSize(qMax(xq_widget_width(m_w), 50), qMax(xq_widget_height(m_w), 50));
}

void
XawViewport::resizeEvent(QResizeEvent *e)
{
	m_area->setGeometry(contentRect());
	XawPeer::resizeEvent(e);
}

/* ------------------------------------------------------------- hooks */

static XawPeer *
make_peer(Widget w)
{
	if (is_menu_object(w))
		return nullptr;
	if (is_shell(w))
		return new XawShell(w);
	Widget parent = xq_widget_parent(w);
	XawPeer *pp = XawPeer::of(parent);
	QWidget *pw = pp;
	if (XawViewport *vp = qobject_cast<XawViewport *>(pp))
		pw = vp->childArea();
	XawPeer *p;
	const char *c = xq_widget_class(w);
	if (!strcmp(c, "AsciiText"))
		p = new XawText(w, pw);
	else if (!strcmp(c, "List") || !strcmp(c, "FigList"))
		p = new XawList(w, pw);
	else if (!strcmp(c, "Scrollbar"))
		p = new XawScrollbar(w, pw);
	else if (!strcmp(c, "Viewport"))
		p = new XawViewport(w, pw);
	else {
		XawPeer::Kind k = XawPeer::Generic;
		if (!strcmp(c, "Label"))		k = XawPeer::Label;
		else if (!strcmp(c, "Command"))		k = XawPeer::Command;
		else if (!strcmp(c, "Toggle"))		k = XawPeer::Toggle;
		else if (!strcmp(c, "MenuButton"))	k = XawPeer::MenuButton;
		else if (!strcmp(c, "Form") || !strcmp(c, "Dialog")) k = XawPeer::Form;
		else if (!strcmp(c, "Box"))		k = XawPeer::Box;
		else if (!strcmp(c, "Paned"))		k = XawPeer::Paned;
		p = new XawPeer(w, k, pw);
	}
	if (XawViewport *vp = qobject_cast<XawViewport *>(pp))
		vp->setChild(p);
	return p;
}

static void
hook_created(Widget w)
{
	/* remember the sizes the program asked for: Xaw keeps them */
	if (xq_widget_width(w) > 0)
		XtVaSetValues(w, "xqW", (XtArgVal)xq_widget_width(w), NULL);
	if (xq_widget_height(w) > 0)
		XtVaSetValues(w, "xqH", (XtArgVal)xq_widget_height(w), NULL);
	XawPeer *p = make_peer(w);
	if (!p)
		return;
	if (!p->adopted())	/* the parent's layout shows it */
		p->setVisible(false);
	size_widget(w);
	if (XawPeer *pp = XawPeer::of(xq_widget_parent(w)))
		pp->scheduleLayout();
}

static void
hook_changed(Widget w, ArgList args, Cardinal n)
{
	XawPeer *p = XawPeer::of(w);
	if (!p)
		return;
	for (Cardinal i = 0; i < n; ++i) {
		if (!args[i].name)
			continue;
		if (!strcmp(args[i].name, XtNwidth) && args[i].value)
			XtVaSetValues(w, "xqW", args[i].value, NULL);
		if (!strcmp(args[i].name, XtNheight) && args[i].value)
			XtVaSetValues(w, "xqH", args[i].value, NULL);
	}
	if (XawText *t = qobject_cast<XawText *>(p))
		t->sync();
	else
		p->sync();
	if (XawList *l = qobject_cast<XawList *>(p)) {
		for (Cardinal i = 0; i < n; ++i)
			if (args[i].name && !strcmp(args[i].name, XtNlist))
				l->setStrings((String *)args[i].value,
					int(xq_res_default(w, XtNnumberStrings, 0)));
	}
	if (XawShell *sh = qobject_cast<XawShell *>(p->window()))
		if (sh->isVisible())
			p->scheduleLayout();
}

static void
hook_managed(Widget w, Boolean m)
{
	XawPeer *p = XawPeer::of(w);
	if (!p)
		return;
	if (p->adopted()) {
		p->setVisible(m);
		if (m && !p->hostManaged())
			p->layoutChildren();
		return;
	}
	if (!m)
		p->hide();
	if (XawPeer *pp = XawPeer::of(xq_widget_parent(w)))
		pp->scheduleLayout();
	else if (m && !p->hostManaged())
		p->show();
}

static void
hook_sensitive(Widget w, Boolean s)
{
	(void)s;
	if (XawPeer *p = XawPeer::of(w)) {
		p->setEnabled(XtIsSensitive(w));
		/* insensitivity is inherited by the children */
		for (QWidget *c : p->findChildren<QWidget *>())
			c->update();
	}
}

static void
hook_geometry(Widget w)
{
	XawPeer *p = XawPeer::of(w);
	if (!p || p->hostManaged()) {
		/* the main window lays these out; xfig may still have
		   changed its canvas size (resize_all) */
		QTimer::singleShot(0, [] {
			if (XawPeer *c = XawPeer::of(xqb_canvas_widget())) {
				QSize s = c->contentSize();
				xqb_canvas_sync(s.width(), s.height());
			}
		});
		return;
	}
	XtVaSetValues(w, "xqW", (XtArgVal)xq_widget_width(w), "xqH",
			(XtArgVal)xq_widget_height(w), NULL);
	XawPeer *pp = XawPeer::of(xq_widget_parent(w));
	if (pp && !p->adopted())
		pp->scheduleLayout();
	else
		p->scheduleLayout();
}

static void
hook_popup(Widget w, XtGrabKind k)
{
	if (!strcmp(xq_widget_class(w), "SimpleMenu")) {
		/* run the menu after the current event */
		QTimer::singleShot(0, [w] { if (xq_widget_popped(w)) popup_simple_menu(w); });
		return;
	}
	if (XawShell *sh = qobject_cast<XawShell *>(XawPeer::of(w)))
		sh->popup(k);
}

static void
hook_popdown(Widget w)
{
	if (XawShell *sh = qobject_cast<XawShell *>(XawPeer::of(w)))
		sh->popdown();
}

static void
hook_destroyed(Widget w)
{
	XawPeer *p = XawPeer::of(w);
	if (!p)
		return;
	xq_widget_set_peer(w, nullptr);
	p->hide();
	p->deleteLater();
}

static Window
hook_window(Widget w)
{
	int wd = qMax(xq_widget_width(w), 1), ht = qMax(xq_widget_height(w), 1);
	XawPeer *p = XawPeer::of(w);
	Window win = xq_create_window(wd, ht, p ? p->devicePixelRatioF() : qApp->devicePixelRatio());
	if (p)
		by_window.insert(win, p);
	return win;
}

static void
hook_focus(Widget w)
{
	if (XawPeer *p = XawPeer::of(w))
		p->setFocus();
}

static void
hook_to_global(Widget w, int *x, int *y)
{
	if (XawPeer *p = XawPeer::of(w)) {
		if (p->isVisible()) {
			QPoint g = p->mapToGlobal(QPoint(*x, *y));
			*x = g.x();
			*y = g.y();
		} else if (QWidget *a = QApplication::activeWindow()) {
			QPoint g = a->mapToGlobal(QPoint(*x, *y));
			*x = g.x();
			*y = g.y();
		}
	}
}

static const char *
hook_get_string(Widget w)
{
	if (XawText *t = qobject_cast<XawText *>(XawPeer::of(w)))
		return t->text();
	return nullptr;
}

static void
hook_text_replace(Widget w, long from, long to, const char *text)
{
	if (XawText *t = qobject_cast<XawText *>(XawPeer::of(w)))
		t->replace(from, to, QString::fromUtf8(text));
}

static long
hook_text_insertion(Widget w)
{
	if (XawText *t = qobject_cast<XawText *>(XawPeer::of(w)))
		return t->insertion();
	return 0;
}

static void
hook_set_insertion(Widget w, long pos)
{
	if (XawText *t = qobject_cast<XawText *>(XawPeer::of(w)))
		t->setInsertion(pos);
}

static Boolean
hook_get_state(Widget w)
{
	return Boolean(xq_res_default(w, XtNstate, False));
}

/* the members of w's radio group */
static QList<Widget>
radio_group(Widget w)
{
	QList<Widget> g;
	Widget leader = (Widget)xq_res_default(w, XtNradioGroup, 0);
	for (XawPeer *t : toggles) {
		Widget tw = t->xt();
		Widget tl = (Widget)xq_res_default(tw, XtNradioGroup, 0);
		if (tw == w || tw == leader || tl == w || (leader && tl == leader))
			g << tw;
	}
	return g;
}

static XtPointer
hook_toggle_current(Widget w)
{
	for (Widget t : radio_group(w))
		if (xq_res_default(t, XtNstate, False)) {
			XtArgVal d;
			return xq_res(t, XtNradioData, &d) ? (XtPointer)d :
				(XtPointer)xq_widget_name(t);
		}
	return nullptr;
}

static void
hook_toggle_set_current(Widget w, XtPointer data)
{
	for (Widget t : radio_group(w)) {
		XtArgVal d;
		XtPointer td = xq_res(t, XtNradioData, &d) ? (XtPointer)d :
			(XtPointer)xq_widget_name(t);
		Boolean on = data && td == data;
		if (Boolean(xq_res_default(t, XtNstate, False)) != on) {
			XtVaSetValues(t, XtNstate, (XtArgVal)on, NULL);
			xq_call_callbacks(t, XtNcallback, (XtPointer)(intptr_t)on);
		}
	}
}

static void
hook_list_change(Widget w, String *list, int n)
{
	if (XawList *l = qobject_cast<XawList *>(XawPeer::of(w)))
		l->setStrings(list, n);
}

static void
hook_list_highlight(Widget w, int i)
{
	if (XawList *l = qobject_cast<XawList *>(XawPeer::of(w)))
		l->highlight(i);
}

static int
hook_list_current(Widget w)
{
	if (XawList *l = qobject_cast<XawList *>(XawPeer::of(w)))
		return l->current();
	return XAW_LIST_NONE;
}

static void
hook_scrollbar_thumb(Widget w, float top, float shown)
{
	if (XawScrollbar *s = qobject_cast<XawScrollbar *>(XawPeer::of(w)))
		s->setThumb(top, shown);
}

static const char *
hook_clipboard_text(void)
{
	static QByteArray text;
	text = QGuiApplication::clipboard()->text().toUtf8();
	return text.constData();
}

void
XawPeer::installHooks()
{
	xq_hooks.created = hook_created;
	xq_hooks.changed = hook_changed;
	xq_hooks.managed = hook_managed;
	xq_hooks.sensitive = hook_sensitive;
	xq_hooks.geometry = hook_geometry;
	xq_hooks.popup = hook_popup;
	xq_hooks.popdown = hook_popdown;
	xq_hooks.destroyed = hook_destroyed;
	xq_hooks.window = hook_window;
	xq_hooks.focus = hook_focus;
	xq_hooks.to_global = hook_to_global;
	xq_hooks.get_string = hook_get_string;
	xq_hooks.text_replace = hook_text_replace;
	xq_hooks.text_insertion = hook_text_insertion;
	xq_hooks.set_insertion = hook_set_insertion;
	xq_hooks.get_state = hook_get_state;
	xq_hooks.toggle_current = hook_toggle_current;
	xq_hooks.toggle_set_current = hook_toggle_set_current;
	xq_hooks.list_change = hook_list_change;
	xq_hooks.list_highlight = hook_list_highlight;
	xq_hooks.list_current = hook_list_current;
	xq_hooks.scrollbar_thumb = hook_scrollbar_thumb;
	xq_hooks.clipboard_text = hook_clipboard_text;

	xq_set_damage_callback([](Window w) {
		if (XawPeer *p = XawPeer::forWindow(w))
			p->markDrawn();
	});
	xq_set_cursor_callback([](Window w, Cursor c) {
		if (XawPeer *p = XawPeer::forWindow(w))
			p->setCursor(xq_qcursor(c));
	});
}
