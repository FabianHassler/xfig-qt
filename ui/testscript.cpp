/*
 * testscript.cpp - drive xfig with synthesized Qt input, for testing.
 *
 * XFIGQT_SCRIPT names a file of commands, one per line:
 *	mode <n>			press the mode button for mode n (F_BOX = 7 ...)
 *	click <x> <y> [button]		click on the canvas (button 1, 2 or 3)
 *	drag <x1> <y1> <x2> <y2> [button]
 *	move <x> <y>
 *	key <text>			type text on the canvas ("\r" etc. allowed)
 *	widget <name> [index]		click the index-th xfig widget of that name
 *	menu <menu> <entry prefix>	trigger a menu entry
 *	wait <ms>
 *	snap <file.png>			save the main window
 *	snapcanvas <file.png>		save the drawing canvas only
 *	snapwin <file.png>		save the topmost other window (dialog)
 *	save <file.fig>
 *	open|merge|saveas <file.fig>	File menu, as after the file dialog
 *	printpdf <file.pdf>		print, as after "Save as PDF" in the dialog
 *	export <file> <lang>		File > Export, as after the dialog; lang
 *					is xfig's language number (mode.h)
 *	datadir				print the data directory in use
 *	quit
 */
/* before xq.h: the X11 shim defines GrayScale, a QPrinter enum value */
#include <QtPrintSupport/QPrinter>

#include "mainwindow.h"
#include "bridge.h"
#include "print.h"

extern "C" const char *xq_data_path(const char *sub);

static QWidget *
canvas()
{
	return XawPeer::of(xqb_canvas_widget());
}

static void
mouse(QWidget *w, QEvent::Type t, QPointF p, Qt::MouseButton b, Qt::MouseButtons bs)
{
	QMouseEvent e(t, p, w->mapToGlobal(p), b, bs, Qt::NoModifier);
	QApplication::sendEvent(w, &e);
}

static Qt::MouseButton
qbutton(int b)
{
	return b == 2 ? Qt::MiddleButton : b == 3 ? Qt::RightButton : Qt::LeftButton;
}

static void
click(QWidget *w, QPointF p, int b)
{
	mouse(w, QEvent::MouseMove, p, Qt::NoButton, Qt::NoButton);
	mouse(w, QEvent::MouseButtonPress, p, qbutton(b), qbutton(b));
	mouse(w, QEvent::MouseButtonRelease, p, qbutton(b), Qt::NoButton);
}

static Widget
find_widget(Widget root, const char *name, int &index)
{
	if (!root)
		return nullptr;
	if (!strcmp(xq_widget_name(root), name) && index-- == 0)
		return root;
	for (int i = 0; i < xq_widget_num_children(root); ++i)
		if (Widget f = find_widget(xq_widget_child(root, i), name, index))
			return f;
	return nullptr;
}

QList<Widget>
xqb_root_widgets()
{
	QList<Widget> roots{xqb_tool_widget()};
	for (QWidget *w : QApplication::topLevelWidgets())
		if (XawPeer *p = qobject_cast<XawPeer *>(w))
			if (p->kind() == XawPeer::Shell)
				roots << p->xt();
	return roots;
}

static void
run(QStringList lines)
{
	while (!lines.isEmpty()) {
		QString line = lines.takeFirst().trimmed();
		if (line.isEmpty() || line.startsWith('#'))
			continue;
		QStringList a = line.split(' ', Qt::SkipEmptyParts);
		QString cmd = a.value(0);
		auto num = [&](int i, double d = 0) { return a.size() > i ? a[i].toDouble() : d; };
		if (cmd == "wait") {
			QTimer::singleShot(int(num(1)), [lines] { run(lines); });
			return;
		} else if (cmd == "mode") {
			Widget b = xqb_mode_button(int(num(1)));
			if (XawPeer *p = XawPeer::of(b))
				click(p, QPointF(p->width() / 2, p->height() / 2), 1);
			else
				qWarning("no mode button %d", int(num(1)));
		} else if (cmd == "click") {
			click(canvas(), QPointF(num(1), num(2)), int(num(3, 1)));
		} else if (cmd == "move") {
			mouse(canvas(), QEvent::MouseMove, QPointF(num(1), num(2)),
					Qt::NoButton, Qt::NoButton);
		} else if (cmd == "drag") {
			QWidget *c = canvas();
			Qt::MouseButton b = qbutton(int(num(5, 1)));
			QPointF p1(num(1), num(2)), p2(num(3), num(4));
			mouse(c, QEvent::MouseMove, p1, Qt::NoButton, Qt::NoButton);
			mouse(c, QEvent::MouseButtonPress, p1, b, b);
			for (int i = 1; i <= 10; ++i)
				mouse(c, QEvent::MouseMove, p1 + (p2 - p1) * i / 10.0, b, b);
			mouse(c, QEvent::MouseButtonRelease, p2, b, Qt::NoButton);
		} else if (cmd == "key") {
			QString text = line.mid(4).replace("\\r", "\r").replace("\\n", "\n");
			for (QChar ch : text) {
				int key = ch == '\r' ? Qt::Key_Return : ch.toUpper().unicode();
				QString t = ch == '\r' ? QString("\r") : QString(ch);
				QKeyEvent press(qtev::KeyPressEvent, key, Qt::NoModifier, t);
				QApplication::sendEvent(canvas(), &press);
				QKeyEvent release(qtev::KeyReleaseEvent, key, Qt::NoModifier, t);
				QApplication::sendEvent(canvas(), &release);
			}
		} else if (cmd == "widget") {
			int index = int(num(2));
			QByteArray n = a.value(1).toLatin1();
			Widget w = nullptr;
			for (Widget root : xqb_root_widgets())
				if ((w = find_widget(root, n.constData(), index)))
					break;
			if (XawPeer *p = XawPeer::of(w))
				click(p, QPointF(p->width() / 2, p->height() / 2), 1);
			else
				qWarning("no widget %s", n.constData());
		} else if (cmd == "menu") {
			QString m = a.value(1), entry = line.section(' ', 2);
			bool found = false;
			for (QAction *top : MainWindow::instance()->menuBar()->actions()) {
				if (top->text() != m || !top->menu())
					continue;
				emit top->menu()->aboutToShow();
				for (QAction *e : top->menu()->actions())
					if (!e->isSeparator() && e->text().startsWith(entry)) {
						e->trigger();
						found = true;
						break;
					}
			}
			if (!found)
				qWarning("no menu entry %s / %s", qPrintable(m), qPrintable(entry));
		} else if (cmd == "snap") {
			MainWindow::instance()->grab().save(a.value(1));
		} else if (cmd == "snapcanvas") {
			canvas()->grab().toImage().save(a.value(1));
		} else if (cmd == "snapwin") {
			/* snapwin <file> [xfig shell name] */
			for (QWidget *w : QApplication::topLevelWidgets()) {
				XawPeer *p = qobject_cast<XawPeer *>(w);
				if (!p || !w->isVisible() || (w->windowType() == Qt::ToolTip))
					continue;
				if (a.size() > 2 && a[2] != xq_widget_name(p->xt()))
					continue;
				w->grab().save(a.value(1));
				break;
			}
		} else if (cmd == "tree") {
			int index = 0;
			QByteArray n = a.value(1).toLatin1();
			Widget w = nullptr;
			for (Widget root : xqb_root_widgets())
				if ((w = find_widget(root, n.constData(), index)))
					break;
			std::function<void(Widget, int)> dump = [&](Widget x, int d) {
				XawPeer *p = XawPeer::of(x);
				XtArgVal fh = 0, fv = 0;
				xq_res(x, XtNfromHoriz, &fh);
				xq_res(x, XtNfromVert, &fv);
				qWarning("%*s%s (%s) %s geom=%d,%d %dx%d qt=%s fromH=%s fromV=%s label=[%s] fg=%lx bg=%lx",
					2 * d, "", xq_widget_name(x), xq_widget_class(x),
					XtIsManaged(x) ? "M" : "u", xq_widget_x(x), xq_widget_y(x),
					xq_widget_width(x), xq_widget_height(x),
					p ? (p->isVisible() ? "vis" : "hid") : "-",
					fh ? xq_widget_name((Widget)fh) : "", fv ? xq_widget_name((Widget)fv) : "",
					xq_widget_label(x), (long)xq_res_default(x, XtNforeground, -1),
					(long)xq_res_default(x, XtNbackground, -1));
				for (int i = 0; i < xq_widget_num_children(x); ++i)
					dump(xq_widget_child(x, i), d + 1);
			};
			if (w)
				dump(w, 0);
		} else if (cmd == "windows") {
			for (QWidget *w : QApplication::topLevelWidgets()) {
				XawPeer *p = qobject_cast<XawPeer *>(w);
				qWarning("window %s %s visible=%d %dx%d",
					p ? xq_widget_name(p->xt()) : "-",
					w->metaObject()->className(), w->isVisible(),
					w->width(), w->height());
			}
		} else if (cmd == "save") {
			xqb_save_file(a.value(1).toLocal8Bit().constData());
		} else if (cmd == "open") {
			MainWindow::instance()->openPath(a.value(1));
		} else if (cmd == "merge") {
			MainWindow::instance()->mergePath(a.value(1));
		} else if (cmd == "saveas") {
			MainWindow::instance()->saveAsPath(a.value(1));
		} else if (cmd == "export") {
			MainWindow::instance()->exportPath(a.value(1), a.value(2).toInt());
		} else if (cmd == "datadir") {
			qWarning("datadir %s", xq_data_path(""));
		} else if (cmd == "printpdf") {
			/* only to a file: a test must not reach a real printer */
			QPrinter printer(QPrinter::HighResolution);
			printer.setOutputFormat(QPrinter::PdfFormat);
			printer.setOutputFileName(a.value(1));
			print_figure_with(printer, nullptr);
		} else if (cmd == "close") {
			/* like the window's close button */
			QTimer::singleShot(0, [] { MainWindow::instance()->close(); });
		} else if (cmd == "quit") {
			xq_exit(0);
		} else {
			qWarning("unknown script command: %s", qPrintable(line));
		}
	}
}

void
run_test_script(const QString &path)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) {
		qWarning("cannot read %s", qPrintable(path));
		return;
	}
	QStringList lines = QString::fromUtf8(f.readAll()).split('\n');
	QTimer::singleShot(500, [lines] { run(lines); });
}
