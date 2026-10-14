/*
 * main.cpp - start the Qt application, then run xfig's own main().
 *
 * xfig's main() creates its (null) widgets, initialises everything and
 * enters its event loop, which processes Qt events (see platform/xt_qt.cpp).
 */
#include "mainwindow.h"
#include "bridge.h"
#include "print.h"

void run_test_script(const QString &path);

/* xfig's data directory (CompKeyDB, doc/, Libraries/): XFIGQT_DATADIR, in
   the macOS app bundle, ../share/xfig-qt from the program (installed on
   Linux), or data/ in the source tree */
static QString
data_dir()
{
	static QString dir;
	if (!dir.isNull())
		return dir;
	QString app = QCoreApplication::applicationDirPath();
	QStringList candidates = {
		qEnvironmentVariable("XFIGQT_DATADIR"),
		app + "/../Resources",
		app + "/../share/xfig-qt",
		QStringLiteral(XFIGQT_SOURCE_DATADIR),
	};
	for (const QString &c : candidates)
		if (!c.isEmpty() && QFileInfo::exists(c + "/CompKeyDB"))
			return dir = QDir(c).absolutePath();
	qWarning("xfig data directory (with CompKeyDB) not found");
	return dir = QString("");
}

extern "C" const char *
xq_data_path(const char *sub)
{
	static QHash<QByteArray, QByteArray> paths;
	QByteArray &p = paths[sub];
	if (p.isEmpty()) {
		QString d = data_dir();
		p = QFile::encodeName(*sub ? d + "/" + QString::fromUtf8(sub) : d);
	}
	return p.constData();
}

static void
created_hook(Widget w, const char *name, const char *wclass)
{
	MainWindow::instance()->widgetCreated(w, name, wclass);
}

static void
setvalues_hook(Widget w, ArgList args, Cardinal n)
{
	MainWindow::instance()->widgetSetValues(w, args, n);
}

int
main(int argc, char **argv)
{
	QApplication app(argc, argv);
	QApplication::setApplicationName("Xfig");
	QApplication::setOrganizationDomain("xfig.org");
	/* xfig's icons and colors are made for a light background */
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
	QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
#endif
	/* a light gray for the panels, white stays for the canvas and fields */
	QPalette pal = QApplication::palette();
	pal.setColor(QPalette::Window, QColor(0xdc, 0xdc, 0xdc));
	pal.setColor(QPalette::Button, QColor(0xec, 0xec, 0xec));
	pal.setColor(QPalette::Base, Qt::white);
	pal.setColor(QPalette::Mid, QColor(0xa8, 0xa8, 0xa8));
	pal.setColor(QPalette::Dark, QColor(0x80, 0x80, 0x80));
	QApplication::setPalette(pal);

	MainWindow window;
	xq_widget_created_hook = created_hook;
	xq_widget_setvalues_hook = setvalues_hook;
	XawPeer::installHooks();
	window.resize(1200, 850);
	window.show();

	/* for testing: XFIGQT_SNAPSHOT=file.png saves the window and quits */
	if (qEnvironmentVariableIsSet("XFIGQT_SNAPSHOT")) {
		QTimer::singleShot(qEnvironmentVariableIntValue("XFIGQT_SNAPSHOT_MS") > 0 ?
				qEnvironmentVariableIntValue("XFIGQT_SNAPSHOT_MS") : 2000, [&window] {
			window.grab().save(qEnvironmentVariable("XFIGQT_SNAPSHOT"));
			xq_exit(0);
		});
	}

	/* for testing: with XFIGQT_GRAB_DIR set, creating the file "grab" in
	   that directory saves all visible windows there as PNG files */
	if (qEnvironmentVariableIsSet("XFIGQT_GRAB_DIR")) {
		QTimer *t = new QTimer(&app);
		QObject::connect(t, &QTimer::timeout, [] {
			QDir dir(qEnvironmentVariable("XFIGQT_GRAB_DIR"));
			if (!dir.exists("grab"))
				return;
			dir.remove("grab");
			int n = 0;
			for (QWidget *w : QApplication::topLevelWidgets())
				if (w->isVisible() && w->windowType() != Qt::ToolTip)
					w->grab().save(dir.filePath(QString("win%1-%2.png")
						.arg(n++).arg(w->objectName().isEmpty() ?
							w->metaObject()->className() : w->objectName())));
		});
		t->start(500);
	}

	if (qEnvironmentVariableIsSet("XFIGQT_SCRIPT"))
		run_test_script(qEnvironmentVariable("XFIGQT_SCRIPT"));

	/* defaults that differ from the X11 version (the command line wins) */
	xq_set_resource("splash", "False");
	xq_set_resource("library_dir", xq_data_path("Libraries"));
	/* EPS and PDF pictures: an app started from the Finder has no
	   Homebrew in its PATH */
	if (!find_ghostscript().isEmpty())
		xq_set_resource("ghostscript", QFile::encodeName(find_ghostscript()));
#ifdef Q_OS_MACOS
	/* fig2dev and Ghostscript, when started from the Finder */
	QByteArray path = qgetenv("PATH");
	for (const char *dir : {"/usr/local/bin", "/opt/homebrew/bin"})
		if (!path.split(':').contains(dir))
			path = dir + QByteArray(":") + path;
	qputenv("PATH", path);
#endif
	/* xfig's temporary files: XFIGTMPDIR, TMPDIR, else Qt's temp dir */
	if (!qEnvironmentVariableIsSet("XFIGTMPDIR"))
		qputenv("XFIGTMPDIR", QFile::encodeName(QDir::tempPath()));

	/* xfig parses the command line itself; it does not return */
	return xfig_main(argc, argv);
}
