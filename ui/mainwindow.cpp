/*
 * mainwindow.cpp - the xfig main window.
 *
 * xfig builds its main window from Xaw widgets.  The panels get Qt peers
 * (xaw.cpp); this window arranges them as in xfig: mode panel on the left,
 * rulers and canvas in the middle, depth manager on the right, indicator
 * panel below.  The command panel's menus become the native menu bar.
 */
#include "mainwindow.h"
#include "bridge.h"
#include "print.h"

MainWindow *MainWindow::s_instance = nullptr;

MainWindow::MainWindow()
{
	s_instance = this;
	setWindowTitle("Xfig");
	setUnifiedTitleAndToolBarOnMac(true);

	QWidget *central = new QWidget;
	central->setAutoFillBackground(true);
	QVBoxLayout *outer = new QVBoxLayout(central);
	outer->setContentsMargins(0, 0, 0, 0);
	outer->setSpacing(0);
	/* message line and mouse buttons on top, as in X11 xfig */
	m_top = new QHBoxLayout;
	m_top->setContentsMargins(4, 2, 2, 2);
	m_top->setSpacing(4);
	outer->addLayout(m_top);
	m_message = new QLabel;
	m_message->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_message->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	m_top->addWidget(m_message, 1);

	QHBoxLayout *middle = m_middle = new QHBoxLayout;
	middle->setSpacing(0);
	outer->addLayout(middle, 1);

	m_left = new QVBoxLayout;
	m_left->setContentsMargins(2, 2, 2, 2);
	m_left->setSpacing(2);
	middle->addLayout(m_left);

	/* rulers and canvas, arranged in setupRulers() */
	m_grid = new QGridLayout;
	m_grid->setContentsMargins(0, 0, 0, 0);
	m_grid->setSpacing(0);
	m_grid->setRowStretch(1, 1);
	middle->addLayout(m_grid, 1);

	m_right = new QVBoxLayout;
	m_right->setContentsMargins(2, 2, 2, 2);
	m_right->setSpacing(2);
	middle->addLayout(m_right);

	m_bottom = new QHBoxLayout;
	m_bottom->setContentsMargins(0, 0, 0, 0);
	m_bottom->setSpacing(0);
	outer->addLayout(m_bottom);

	setCentralWidget(central);

	createMenus();
}

/* ------------------------------------------------------------------ menus */

/* "Open...     (Meta-O)" -> "Open...", Ctrl+O (Command-O on macOS) */
static QString
menu_text(const QString &label, QKeySequence *key)
{
	QString t = label.trimmed();
	static const QRegularExpression rx("\\((Shift-)?(Meta|Ctrl)-([A-Za-z])\\)");
	QRegularExpressionMatch m = rx.match(t);
	if (m.hasMatch() && key) {
		Qt::Key k = Qt::Key(m.captured(3).toUpper().at(0).unicode());
		Qt::KeyboardModifiers mods = Qt::ControlModifier;
		if (!m.captured(1).isEmpty())
			mods |= Qt::ShiftModifier;
		/* xfig's own Ctrl- accelerators become Command-Option */
		if (m.captured(2) == "Ctrl")
			mods |= Qt::AltModifier;
		*key = QKeySequence(QKeyCombination(mods, k));
	}
	/* the shortcuts only: "(HTML)" and "(PDF)" in Help stay */
	static const QRegularExpression paren("\\s*\\((Quick = [^)]*|"
			"(Shift-|Meta-|Ctrl-)+[^)]*|F\\d+(/F\\d+)?|[A-Za-z])\\)");
	t.remove(paren);
	return t.trimmed();
}

void
MainWindow::createMenus()
{
	for (int i = 0; i < xqb_num_menus(); ++i) {
		QMenu *qm = menuBar()->addMenu(QString::fromLatin1(xqb_menu_title(i)));
		m_menus << qm;
		connect(qm, &QMenu::aboutToShow, this, [this, qm, i] { fillMenu(qm, i); });
	}
	/* the entries exist once xfig made its menus: fill in the shortcuts */
	QTimer::singleShot(0, this, [this] {
		for (int i = 0; i < m_menus.size(); ++i)
			fillMenu(m_menus[i], i);
	});
}

static bool
bitmap_blank(Pixmap p)
{
	QImage *img = p ? xq_image(p) : nullptr;
	if (!img)
		return true;
	for (int y = 0; y < img->height(); ++y)
		for (int x = 0; x < img->width(); ++x)
			if ((img->pixel(x, y) & 0xffffff) != 0)
				return false;
	return true;
}

void
MainWindow::fillMenu(QMenu *qm, int index)
{
	Widget menu = xqb_menu_widget(index);
	if (!menu)
		return;
	qm->clear();
	for (int i = 0; i < xq_widget_num_children(menu); ++i) {
		Widget e = xq_widget_child(menu, i);
		if (!XtIsManaged(e))
			continue;
		if (!strcmp(xq_widget_class(e), "SmeLine")) {
			qm->addSeparator();
			continue;
		}
		QKeySequence key;
		QString text = menu_text(QString::fromUtf8(xq_widget_label(e)), &key);
		QAction *a = qm->addAction(text);
		a->setShortcut(key);
		a->setEnabled(XtIsSensitive(e));
		Pixmap lb = Pixmap(xq_res_default(e, XtNleftBitmap, 0));
		/* xfig marks the selected view options with a left bitmap */
		if (text.startsWith("Show") || text.startsWith("Autorefresh")) {
			a->setCheckable(true);
			a->setChecked(!bitmap_blank(lb));
		}
		if (text == "Exit")
			a->setMenuRole(QAction::QuitRole);
		else if (text.startsWith("About"))
			a->setMenuRole(QAction::AboutRole);
		else if (text.startsWith("Global settings"))
			a->setMenuRole(QAction::PreferencesRole);
		else
			a->setMenuRole(QAction::NoRole);
		/* the macOS file dialog instead of xfig's file panel */
		if (text == "Open...") {
			connect(a, &QAction::triggered, this, &MainWindow::openFile);
			continue;
		}
		if (text == "Merge...") {
			connect(a, &QAction::triggered, this, &MainWindow::mergeFile);
			continue;
		}
		if (text == "Save As...") {
			connect(a, &QAction::triggered, this, &MainWindow::saveFileAs);
			continue;
		}
		if (text == "Export...") {
			/* the save dialog with xfig's formats; xfig's export
			   panel keeps its options (magnification, margin...) */
			connect(a, &QAction::triggered, this, &MainWindow::exportFile);
			QAction *o = qm->addAction("Export Options...");
			o->setEnabled(XtIsSensitive(e));
			o->setMenuRole(QAction::NoRole);
			connect(o, &QAction::triggered, this, [e] {
				xq_call_callbacks(e, XtNcallback, nullptr);
			});
			continue;
		}
		if (text == "Print...") {
			/* the macOS print dialog; xfig's print panel keeps
			   its options (magnification, layers, grid...) */
			connect(a, &QAction::triggered, this, [this] { print_figure(this); });
			QAction *o = qm->addAction("Print Options...");
			o->setEnabled(XtIsSensitive(e));
			o->setMenuRole(QAction::NoRole);
			connect(o, &QAction::triggered, this, [e] {
				xq_call_callbacks(e, XtNcallback, nullptr);
			});
			continue;
		}
		if (text == "Save") {
			/* a figure without a name needs the save dialog */
			connect(a, &QAction::triggered, this, [this, e] {
				if (*xqb_current_path())
					xq_call_callbacks(e, XtNcallback, nullptr);
				else
					saveFileAs();
			});
			continue;
		}
		connect(a, &QAction::triggered, this, [e] {
			xq_call_callbacks(e, XtNcallback, nullptr);
		});
	}
	/* no separator at the end */
	while (!qm->actions().isEmpty() && qm->actions().last()->isSeparator())
		qm->removeAction(qm->actions().last());
}

/* ----------------------------------------------------------------- rulers */

/* As in xfig, the side ruler is right of the canvas and the unit box ends
   the top ruler's row.  With -right (the mode panel on the right) the
   side ruler and the unit box are left of the canvas and the top ruler.
   Called with the first of these widgets, when xfig has read its options. */
void
MainWindow::setupRulers()
{
	if (m_rulersSetUp)
		return;
	m_rulersSetUp = true;
	m_rulerRight = !xqb_rhs_panel();
	m_grid->setColumnStretch(m_rulerRight ? 0 : 1, 1);
	if (!m_rulerRight) {
		/* the mode panel goes right of the canvas */
		m_middle->removeItem(m_left);
		m_middle->insertLayout(m_middle->indexOf(m_grid) + 1, m_left);
	}
	if (m_rulerRight) {
		m_rulerRow = new QHBoxLayout;
		m_rulerRow->setContentsMargins(0, 0, 0, 0);
		m_rulerRow->setSpacing(0);
		m_grid->addLayout(m_rulerRow, 0, 0, 1, 2);
	}
}

/* ------------------------------------------------------------ file dialogs */

static const char *fig_filter = "Fig files (*.fig);;All files (*)";

QString
MainWindow::fileDialogDir() const
{
	QString path = QString::fromLocal8Bit(xqb_current_path());
	if (!path.isEmpty())
		return path;
	return QString::fromLocal8Bit(xqb_current_dir());
}

/* before the figure is replaced: Save, Don't Save or Cancel */
bool
MainWindow::maybeSave()
{
	if (!xqb_needs_saving())
		return true;
	QString name = QFileInfo(QString::fromLocal8Bit(xqb_current_path())).fileName();
	QMessageBox box(QMessageBox::Warning, "Xfig",
		name.isEmpty() ? QString("Do you want to save the changes to this figure?")
			: QString("Do you want to save the changes to \"%1\"?").arg(name),
		QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
	box.setInformativeText("Your changes will be lost if you don't save them.");
	box.setDefaultButton(QMessageBox::Save);
	switch (box.exec()) {
	case QMessageBox::Save:
		if (*xqb_current_path())
			return saveAsPath(QString::fromLocal8Bit(xqb_current_path()));
		return saveFileAs();
	case QMessageBox::Discard:
		return true;
	default:
		return false;
	}
}

void
MainWindow::openFile()
{
	if (xqb_busy() || !maybeSave())
		return;
	QString path = QFileDialog::getOpenFileName(this, "Open",
			fileDialogDir(), fig_filter);
	if (!path.isEmpty())
		openPath(path);
}

void
MainWindow::mergeFile()
{
	if (xqb_busy())
		return;
	QString path = QFileDialog::getOpenFileName(this, "Merge",
			fileDialogDir(), fig_filter);
	if (!path.isEmpty())
		mergePath(path);
}

bool
MainWindow::saveFileAs()
{
	if (xqb_busy())
		return false;
	QString start = fileDialogDir();
	if (QFileInfo(start).isDir())
		start += "/untitled.fig";
	QString path = QFileDialog::getSaveFileName(this, "Save As", start,
			fig_filter);
	if (path.isEmpty())
		return false;
	return saveAsPath(path);
}

/* File > Export...: the formats are xfig's export languages */
void
MainWindow::exportFile()
{
	if (xqb_busy())
		return;
	QStringList filters;
	for (int i = 0; i < xqb_num_export_langs(); ++i)
		filters << QString("%1 (*.%2)").arg(
				QString::fromLatin1(xqb_export_lang_text(i)).simplified(),
				QString::fromLatin1(xqb_export_suffix(i)));
	int lang = xqb_export_lang();
	QFileInfo fig(QString::fromLocal8Bit(xqb_current_path()));
	QString dir = fig.fileName().isEmpty() ? fileDialogDir() : fig.absolutePath();
	QString base = fig.fileName().isEmpty() ? QString("Untitled") : fig.completeBaseName();
	QString selected = filters[lang];
	QString path = QFileDialog::getSaveFileName(this, "Export",
			dir + "/" + base + "." + xqb_export_suffix(lang),
			filters.join(";;"), &selected);
	if (path.isEmpty())
		return;
	exportPath(path, qMax(0, filters.indexOf(selected)));
}

bool
MainWindow::exportPath(const QString &path, int lang)
{
	QString p = QFileInfo(path).absoluteFilePath();
	if (QFileInfo(p).suffix().isEmpty())
		p += QString(".") + xqb_export_suffix(lang);
	return xqb_export(p.toLocal8Bit(), lang) == 0;
}

bool
MainWindow::openPath(const QString &path)
{
	return xqb_open(QFileInfo(path).absoluteFilePath().toLocal8Bit()) == 0;
}

bool
MainWindow::mergePath(const QString &path)
{
	return xqb_merge(QFileInfo(path).absoluteFilePath().toLocal8Bit()) == 0;
}

bool
MainWindow::saveAsPath(const QString &path)
{
	QString p = QFileInfo(path).absoluteFilePath();
	if (QFileInfo(p).suffix().isEmpty())
		p += ".fig";
	return xqb_save_as(p.toLocal8Bit()) == 0;
}

/* ----------------------------------------------------------------- panels */

void
MainWindow::widgetCreated(Widget w, const char *name, const char *wclass)
{
	(void)wclass;
	XawPeer *p = XawPeer::of(w);
	if (!p)
		return;
	QString n = QString::fromLatin1(name);
	Widget parent = xq_widget_parent(w);
	if (qEnvironmentVariableIsSet("XFIGQT_DEBUG"))
		qDebug("created %s (%s) in %s, %dx%d", name, wclass,
			parent ? xq_widget_name(parent) : "-",
			xq_widget_width(w), xq_widget_height(w));
	/* only the panels of the main form, not equally named dialog parts */
	if (!parent || strcmp(xq_widget_name(parent), "form") != 0)
		return;
	static const QStringList adopt = {"canvas", "topruler", "sideruler",
		"unitbox", "mode_panel", "snap_indicator_form", "layer_form",
		"ind_panel", "upd_ctrl_form", "mouse_panel"};
	if (adopt.contains(n))
		p->setAdopted(true);
	if (n == "canvas" || n == "topruler" || n == "sideruler" || n == "unitbox")
		setupRulers();
	if (n == "canvas") {
		p->setParent(nullptr);
		p->setHostManaged(true);
		p->createWindow();
		p->setMinimumSize(100, 100);
		p->setFocusPolicy(Qt::StrongFocus);
		p->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		p->onResize = [](int wd, int ht) { xqb_canvas_resized(wd, ht); };
		m_grid->addWidget(p, 1, m_rulerRight ? 0 : 1);
		p->show();
		p->setFocus();
	} else if (n == "topruler") {
		p->setParent(nullptr);
		p->setHostManaged(true);
		p->createWindow();
		p->setFixedHeight(p->sizeHint().height());
		p->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
		p->onResize = [](int wd, int) { xqb_rulers_resized(wd, 0); };
		if (m_rulerRight)
			m_rulerRow->insertWidget(0, p, 1);
		else
			m_grid->addWidget(p, 0, 1);
		p->show();
	} else if (n == "sideruler") {
		p->setParent(nullptr);
		p->setHostManaged(true);
		p->createWindow();
		p->setFixedWidth(p->sizeHint().width());
		p->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Ignored);
		p->onResize = [](int, int ht) { xqb_rulers_resized(0, ht); };
		m_grid->addWidget(p, 1, m_rulerRight ? 1 : 0);
		p->show();
	} else if (n == "unitbox") {
		/* the corner between the rulers, showing the units */
		p->setParent(nullptr);
		p->setHostManaged(true);
		QFont f = p->font();
		f.setPixelSize(10);
		p->setFont(f);
		if (m_rulerRight) {
			/* as wide as its text, at least as the side ruler */
			p->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Ignored);
			p->setFixedWidth(qMax(p->sizeHint().width(),
				QFontMetrics(f).horizontalAdvance("1in = 1.00in") + 12));
			m_rulerRow->addWidget(p);
		} else {
			p->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
			m_grid->addWidget(p, 0, 0);
		}
		p->show();
	} else if (n == "mode_panel") {
		m_modePanel = p;
		/* xfig's two columns; a window too low for them scrolls */
		m_modeScroll = new QScrollArea;
		m_modeScroll->setFrameShape(QFrame::NoFrame);
		m_modeScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		m_modeScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		m_modeScroll->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
		m_modeScroll->verticalScrollBar()->setSingleStep(12);
		p->setParent(nullptr);
		m_modeScroll->setWidget(p);
		m_left->addWidget(m_modeScroll, 1);
		p->show();
	} else if (n == "snap_indicator_form") {
		/* xfig makes it as tall as the canvas; it needs only two lines */
		XtVaSetValues(w, "xqH", (XtArgVal)0, NULL);
		p->setParent(nullptr);
		m_right->addWidget(p, 0, Qt::AlignTop | Qt::AlignHCenter);
		p->show();
	} else if (n == "layer_form") {
		/* the depth manager fills the rest of the column */
		XtVaSetValues(w, "xqH", (XtArgVal)0, NULL);
		p->setParent(nullptr);
		p->setHostManaged(true);
		p->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
		m_right->addWidget(p, 1);
		p->show();
	} else if (n == "ind_panel") {
		p->setParent(nullptr);
		p->setHostManaged(true);
		p->setFixedHeight(p->sizeHint().height());
		p->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		if (XawViewport *vp = qobject_cast<XawViewport *>(p)) {
			vp->setFitHeight(true);
			/* xfig forces a scrollbar; Xaw drew it as a plain band */
			vp->setHorizontalScrollBarAsNeeded();
		}
		m_bottom->addWidget(p, 1);
		p->show();
	} else if (n == "upd_ctrl_form") {
		p->setParent(nullptr);
		m_bottom->addWidget(p, 0, Qt::AlignTop);
		/* xfig shows it in the update mode only */
		p->setVisible(XtIsManaged(w));
	} else if (n == "mouse_panel") {
		p->setParent(nullptr);
		p->setHostManaged(true);
		p->createWindow();
		p->setFixedSize(p->sizeHint());
		m_top->addWidget(p);
		m_mouseWidget = w;
		p->show();
	} else if (n == "message") {
		m_msgWidget = w;
	}
}

void
MainWindow::widgetSetValues(Widget w, ArgList args, Cardinal n)
{
	for (Cardinal i = 0; i < n; ++i) {
		const char *name = args[i].name;
		if (!name)
			continue;
		if (m_modePanel && w == m_modePanel->xt() && !strcmp(name, XtNwidth)) {
			/* xfig sized it for the screen: fit it to the window again */
			QTimer::singleShot(0, this, [this] { fitModePanel(); });
		} else if (!strcmp(name, XtNheight) && args[i].value &&
				!strcmp(xq_widget_name(w), "label") &&
				!strcmp(xq_widget_name(xq_widget_parent(w)), "mode_panel")) {
			/* xfig pads "Drawing" and "Editing" to fill the panel's
			   height; the Qt layout puts space below the buttons */
			XtVaSetValues(w, "xqH", (XtArgVal)0, NULL);
		} else if (w == m_mouseWidget && (!strcmp(name, XtNheight) ||
					!strcmp(name, XtNwidth))) {
			/* xfig makes the mouse panel taller once it is set up */
			if (XawPeer *p = XawPeer::of(w))
				p->setFixedSize(p->sizeHint());
		} else if (w == m_msgWidget && !strcmp(name, XtNstring)) {
			m_message->setText(QString::fromUtf8((const char *)args[i].value));
		} else if (!strcmp(name, XtNtitle) && args[i].value &&
				!strcmp(xq_widget_name(w), "xfig")) {
			setWindowTitle(QString::fromUtf8((const char *)args[i].value));
			/* the document icon in the macOS title bar */
			setWindowFilePath(QString::fromLocal8Bit(xqb_current_path()));
		}
	}
}

/*
 * The mode panel has xfig's columns: two, or -but_per_row.  (X11 xfig
 * adds columns when the screen is too low at startup; here the panel
 * scrolls in a window too low for it.)
 */
void
MainWindow::fitModePanel()
{
	if (!m_modePanel)
		return;
	Widget mp = m_modePanel->xt();
	/* the column width: the normal buttons (the library one is wider) */
	int bw = INT_MAX;
	for (int i = 0; i < xq_widget_num_children(mp); ++i) {
		Widget c = xq_widget_child(mp, i);
		if (!strcmp(xq_widget_class(c), "Command"))
			bw = qMin(bw, xq_widget_width(c) + 2 * xq_widget_border(c));
	}
	if (bw <= 0 || bw == INT_MAX)
		return;
	int hs = int(xq_res_default(mp, XtNhSpace, 4));
	int cols = qBound(1, xqb_buttons_per_row(), 6);
	int width = cols * (bw + hs) + hs;
	XtVaSetValues(mp, "xqW", (XtArgVal)width, NULL);
	/* the "Drawing" and "Editing" labels span the panel */
	for (int i = 0; i < xq_widget_num_children(mp); ++i) {
		Widget c = xq_widget_child(mp, i);
		if (!strcmp(xq_widget_class(c), "Label"))
			XtVaSetValues(c, "xqW", (XtArgVal)(width - 2 * hs), NULL);
	}
	m_modePanel->layoutChildren();
	QSize size(xq_widget_width(mp), xq_widget_height(mp));
	m_modePanel->resize(size);
	bool scroll = size.height() > m_modeScroll->viewport()->height() &&
			m_modeScroll->height() > 0 && size.height() > m_modeScroll->height();
	m_modeScroll->setFixedWidth(size.width() + (scroll ?
			m_modeScroll->verticalScrollBar()->sizeHint().width() : 0));
	if (qEnvironmentVariableIsSet("XFIGQT_DEBUG"))
		qDebug("mode panel: %d columns, %dx%d in %d", cols, size.width(),
				size.height(), m_modeScroll->height());
}

void
MainWindow::resizeEvent(QResizeEvent *e)
{
	QMainWindow::resizeEvent(e);
	/* after the layout has settled */
	QTimer::singleShot(50, this, [this] { fitModePanel(); });
}

void
MainWindow::closeEvent(QCloseEvent *e)
{
	/* xfig's quit asks to save, then exits */
	e->ignore();
	xqb_quit();
}
