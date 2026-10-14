/*
 * print.cpp - print through the system's print dialog.
 *
 * xfig prints by piping fig2dev's PostScript into lp or lpr, with a typed
 * printer name.  Here xfig writes the same PostScript (its print settings:
 * magnification, paper, orientation, layers) to a file, and the print
 * dialog chooses printer, copies and pages.  Ghostscript makes a PDF of it,
 * which goes to CUPS on macOS and Linux (recent macOS no longer takes
 * PostScript), or to the file "Save as PDF" chose: it stays vector.
 * Windows has no CUPS: Ghostscript renders the pages at the printer's
 * resolution and Qt prints the images.
 */
/* before xq.h: the X11 shim defines GrayScale, a QPrinter enum value */
#include <QtPrintSupport/QPrintDialog>
#include <QtPrintSupport/QPrinter>
#ifndef Q_OS_WIN
#include <cups/cups.h>
#endif

#include "print.h"
#include "bridge.h"
#undef GrayScale

static QString
figure_title()
{
	QString name = QFileInfo(QString::fromLocal8Bit(xqb_current_path())).fileName();
	return name.isEmpty() ? QString("Untitled") : name;
}

/* an app started from the Finder has no Homebrew in its PATH */
static QString
find_tool(const QStringList &names)
{
	for (const QString &n : names) {
		QString p = QStandardPaths::findExecutable(n);
		if (p.isEmpty())
			p = QStandardPaths::findExecutable(n,
					{"/opt/homebrew/bin", "/usr/local/bin"});
		if (!p.isEmpty())
			return p;
	}
	return QString();
}

QString
find_ghostscript()
{
	static QString gs = find_tool({"gs", "gswin64c", "gswin32c"});
	return gs;
}

static bool
run_ghostscript(const QStringList &args, QString *err)
{
	if (find_ghostscript().isEmpty()) {
		*err = "Ghostscript (gs) was not found.";
		return false;
	}
	QProcess p;
	p.start(find_ghostscript(), QStringList{"-q", "-dSAFER", "-dBATCH",
			"-dNOPAUSE"} + args);
	if (!p.waitForFinished(300000) || p.exitStatus() != QProcess::NormalExit ||
			p.exitCode() != 0) {
		*err = QString("Ghostscript failed: %1").arg(
				QString::fromLocal8Bit(p.readAllStandardError()).trimmed());
		return false;
	}
	return true;
}

static bool
ps_to_pdf(const QString &ps, const QString &pdf, QString *err)
{
	return run_ghostscript({"-sDEVICE=pdfwrite", "-sOutputFile=" + pdf, ps}, err);
}

#ifdef Q_OS_WIN
/* the pages as images at the printer's resolution, painted by Qt */
static bool
ps_to_printer(const QString &ps, QPrinter &printer, const QString &dir,
		QString *err)
{
	int dpi = qBound(150, printer.resolution(), 600);
	bool gray = printer.colorMode() == QPrinter::GrayScale;
	if (!run_ghostscript({gray ? "-sDEVICE=pnggray" : "-sDEVICE=png16m",
			QString("-r%1").arg(dpi), "-dTextAlphaBits=4",
			"-dGraphicsAlphaBits=4", "-sOutputFile=" + dir + "/page%03d.png",
			ps}, err))
		return false;
	QStringList pages = QDir(dir).entryList({"page*.png"}, QDir::Files,
			QDir::Name);
	int from = 1, to = pages.size();
	if (printer.printRange() == QPrinter::PageRange) {
		from = qMax(1, printer.fromPage());
		to = qMin(to, printer.toPage());
	}
	printer.setFullPage(true);
	QPainter p;
	if (!p.begin(&printer)) {
		*err = QString("Cannot print on \"%1\".").arg(printer.printerName());
		return false;
	}
	QRectF paper = printer.paperRect(QPrinter::DevicePixel);
	for (int i = from; i <= to; ++i) {
		if (i > from)
			printer.newPage();
		p.drawImage(QRectF(0, 0, paper.width(), paper.height()),
				QImage(QDir(dir).filePath(pages[i - 1])));
	}
	p.end();
	xqb_message(QString("Sent \"%1\" to printer \"%2\"")
			.arg(figure_title(), printer.printerName())
			.toLocal8Bit().constData());
	return true;
}
#else
static bool
pdf_to_cups(const QString &pdf, const QPrinter &printer, QString *err)
{
	cups_option_t	*opt = nullptr;
	int		n = 0;

	QByteArray dest = printer.printerName().toUtf8();
	cups_dest_t *d = cupsGetNamedDest(CUPS_HTTP_DEFAULT, dest.constData(), nullptr);
	if (!d) {
		*err = QString("There is no printer \"%1\".").arg(printer.printerName());
		return false;
	}
	cupsFreeDests(1, d);

	n = cupsAddOption("copies", QByteArray::number(printer.copyCount()), n, &opt);
	if (printer.copyCount() > 1)
		n = cupsAddOption("collate",
				printer.collateCopies() ? "true" : "false", n, &opt);
	const char *sides = "one-sided";
	switch (printer.duplex()) {
	case QPrinter::DuplexLongSide:
	case QPrinter::DuplexAuto:	sides = "two-sided-long-edge"; break;
	case QPrinter::DuplexShortSide:	sides = "two-sided-short-edge"; break;
	default:			break;
	}
	n = cupsAddOption("sides", sides, n, &opt);
	if (printer.printRange() == QPrinter::PageRange)
		n = cupsAddOption("page-ranges", QByteArray::number(printer.fromPage())
				+ "-" + QByteArray::number(printer.toPage()), n, &opt);
	if (printer.colorMode() == QPrinter::GrayScale)
		n = cupsAddOption("print-color-mode", "monochrome", n, &opt);

	int job = cupsPrintFile(dest.constData(), QFile::encodeName(pdf).constData(),
			figure_title().toUtf8().constData(), n, opt);
	cupsFreeOptions(n, opt);
	if (job == 0) {
		*err = QString("Printing on \"%1\" failed: %2").arg(printer.printerName(),
				QString::fromUtf8(cupsLastErrorString()));
		return false;
	}
	xqb_message(QString("Sent \"%1\" to printer \"%2\" (job %3)")
			.arg(figure_title(), printer.printerName()).arg(job)
			.toLocal8Bit().constData());
	return true;
}
#endif

bool
print_figure_with(QPrinter &printer, QWidget *parent)
{
	QTemporaryDir tmp;
	if (!tmp.isValid())
		return false;
	QString ps = tmp.filePath("figure.ps");
	/* xfig reports its own errors in the message panel */
	if (xqb_print_to_file(QFile::encodeName(ps).constData()) != 0)
		return false;

	QString err;
	bool ok;
	if (printer.outputFormat() == QPrinter::PdfFormat &&
			!printer.outputFileName().isEmpty()) {
		ok = ps_to_pdf(ps, printer.outputFileName(), &err);
		if (ok)
			xqb_message(QString("Saved \"%1\" as %2").arg(figure_title(),
				printer.outputFileName()).toLocal8Bit().constData());
	} else {
#ifdef Q_OS_WIN
		ok = ps_to_printer(ps, printer, tmp.path(), &err);
#else
		QString pdf = tmp.filePath("figure.pdf");
		ok = ps_to_pdf(ps, pdf, &err) && pdf_to_cups(pdf, printer, &err);
#endif
	}
	if (!ok && parent)
		QMessageBox::warning(parent, "Print", err);
	else if (!ok)
		qWarning("%s", qPrintable(err));
	return ok;
}

bool
print_figure(QWidget *parent)
{
	if (xqb_busy())
		return false;
	QPrinter printer(QPrinter::HighResolution);
	printer.setDocName(figure_title());
	QPrintDialog dialog(&printer, parent);
	dialog.setWindowTitle("Print");
	dialog.setOption(QAbstractPrintDialog::PrintSelection, false);
	dialog.setOption(QAbstractPrintDialog::PrintCurrentPage, false);
	dialog.setOption(QAbstractPrintDialog::PrintPageRange, true);
	if (dialog.exec() != QDialog::Accepted)
		return false;
	return print_figure_with(printer, parent);
}
