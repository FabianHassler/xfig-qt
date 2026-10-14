/*
 * xaw.h - Qt peers for xfig's Xaw widgets.
 *
 * xfig builds its panels and dialogs from Athena (Xaw) widgets.  Every such
 * widget record (shim/xt_widget.c) gets a Qt peer that shows it with the
 * size and position Xaw would give it, and turns Qt input into X events,
 * which run xfig's event handlers, translations and callbacks.  Simple
 * menus become QMenus, text fields QLineEdits, lists QListWidgets, shells
 * top-level windows.
 */
#ifndef XAW_H
#define XAW_H

#include "xq.h"
#include "xq_widget.h"

class XawPeer : public QWidget
{
	Q_OBJECT
public:
	enum Kind {
		Generic, Label, Command, Toggle, MenuButton, Text, List,
		Scrollbar, Viewport, Form, Box, Paned, Shell
	};

	/* install the hooks into the widget model */
	static void	installHooks();

	static XawPeer	*of(Widget w);
	static XawPeer	*forWindow(Window win);

	XawPeer(Widget w, Kind kind, QWidget *parent = nullptr);
	~XawPeer() override;

	Widget	xt() const { return m_w; }
	Kind	kind() const { return m_kind; }

	/* the size Xaw would give the widget (without the border) */
	virtual QSize	naturalSize() const;

	/* re-read the resources and repaint */
	virtual void	sync();

	/* place the children (containers) */
	void	layoutChildren();
	void	scheduleLayout();

	/* the host (main window) sets the geometry, not the Xaw layout */
	void	setHostManaged(bool h) { m_host = h; }
	bool	hostManaged() const { return m_host; }
	QSize	contentSize() const { return contentRect().size(); }

	/* placed by the host: the Xaw parent does not lay it out */
	void	setAdopted(bool a) { m_adopted = a; }
	bool	adopted() const { return m_adopted; }

	/* called after a resize by Qt, before the Expose event */
	std::function<void(int, int)> onResize;

	/* the window contents were drawn by xfig */
	void	markDrawn() { m_drawn = true; update(); }

	/* in a viewport: make xfig's size the minimum */
	void	fitToViewport();

	/* give the widget a window (an X drawable) of the peer's size */
	Window	createWindow();

	/* send a synthesized X event through handlers and translations */
	int	sendEvent(XEvent &ev);

	/* X event construction */
	static unsigned xState(Qt::KeyboardModifiers m, Qt::MouseButtons b);
	void	makeButton(XEvent &ev, int type, unsigned button, QPointF pos,
			Qt::KeyboardModifiers mods, Qt::MouseButtons buttons);
	bool	makeKey(XEvent &ev, int type, QKeyEvent *e);

	QSize	sizeHint() const override;

protected:
	void	paintEvent(QPaintEvent *) override;
	void	resizeEvent(QResizeEvent *) override;
	void	mousePressEvent(QMouseEvent *) override;
	void	mouseReleaseEvent(QMouseEvent *) override;
	void	mouseMoveEvent(QMouseEvent *) override;
	void	mouseDoubleClickEvent(QMouseEvent *) override;
	void	wheelEvent(QWheelEvent *) override;
	void	keyPressEvent(QKeyEvent *) override;
	void	keyReleaseEvent(QKeyEvent *) override;
	void	enterEvent(QEnterEvent *) override;
	void	leaveEvent(QEvent *) override;
	bool	focusNextPrevChild(bool next) override;

	virtual void	activate(QMouseEvent *e);	/* Command/Toggle release */
	void	paintFace(QPainter &p, QRect r);
	void	paintLabel(QPainter &p, QRect r);
	unsigned xButton(QMouseEvent *e) const;
	QRect	contentRect() const;

	Widget	m_w;
	Kind	m_kind;
	bool	m_host = false;
	bool	m_adopted = false;
	bool	m_drawn = false;
	bool	m_hover = false;
	bool	m_down = false;
	bool	m_layoutPending = false;
	QSize	m_stretch;		/* size given by the parent Form */
	unsigned m_pressed = 0;
	QPointF	m_wheel;
};

/* the top-level window of a shell widget */
class XawShell : public XawPeer
{
	Q_OBJECT
public:
	XawShell(Widget w);
	void	popup(XtGrabKind grab);
	void	popdown();
	void	fitToChild();
protected:
	void	closeEvent(QCloseEvent *) override;
private:
	bool	m_fitting = false;
};

/* AsciiText */
class XawText : public XawPeer
{
	Q_OBJECT
public:
	XawText(Widget w, QWidget *parent);
	const char *text();
	void	replace(long from, long to, const QString &t);
	long	insertion() const;
	void	setInsertion(long pos);
	void	sync() override;
	QSize	naturalSize() const override;
	bool	eventFilter(QObject *o, QEvent *e) override;
protected:
	void	resizeEvent(QResizeEvent *) override;
private:
	QString	current() const;
	void	setCurrent(const QString &s);
	QLineEdit	*m_line = nullptr;
	QPlainTextEdit	*m_multi = nullptr;
	QByteArray	m_utf8;
	bool		m_syncing = false;
};

/* List and FigList */
class XawList : public XawPeer
{
	Q_OBJECT
public:
	XawList(Widget w, QWidget *parent);
	void	setStrings(String *list, int n);
	void	highlight(int i);
	int	current() const;
	QSize	naturalSize() const override;
	bool	eventFilter(QObject *o, QEvent *e) override;
protected:
	void	resizeEvent(QResizeEvent *) override;
private:
	void	notify(int row);
	QListWidget	*m_list;
};

/* Scrollbar */
class XawScrollbar : public XawPeer
{
	Q_OBJECT
public:
	XawScrollbar(Widget w, QWidget *parent);
	void	setThumb(float top, float shown);
	QSize	naturalSize() const override;
protected:
	void	resizeEvent(QResizeEvent *) override;
private:
	QScrollBar	*m_bar;
	bool		m_setting = false;
};

/* Viewport */
class XawViewport : public XawPeer
{
	Q_OBJECT
public:
	XawViewport(Widget w, QWidget *parent);
	void	setChild(XawPeer *child);
	QWidget	*childArea() const;
	/* keep the height of the child plus a scrollbar (host managed) */
	void	setFitHeight(bool f) { m_fit = f; }
	/* a horizontal scrollbar only when the child is wider (space for it
	   stays reserved with setFitHeight) */
	void	setHorizontalScrollBarAsNeeded();
	void	childLaidOut(XawPeer *child);
	QSize	naturalSize() const override;
protected:
	void	resizeEvent(QResizeEvent *) override;
private:
	QScrollArea	*m_area;
	bool		m_fit = false;
};

#endif
