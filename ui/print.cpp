/*
 * print.cpp - print through the system's print dialog.
 *
 * xfig prints by piping fig2dev's PostScript into lp or lpr, with a typed
 * printer name.  Here xfig writes the same PostScript (its print settings:
 * magnification, paper, orientation, layers) to a file, and the print
 * dialog chooses printer, copies and pages.  Ghostscript makes a PDF of it,
 * which goes to CUPS (recent macOS no longer takes PostScript), or to the
 * file "Save as PDF" chose: it stays vector.  Without Ghostscript, Qt
 * prints fig2dev's SVG (svgout.cpp).
 */
/* before xq.h: the X11 shim defines GrayScale, a QPrinter enum value */
#include <QtPrintSupport/QPrintDialog>
#include <QtPrintSupport/QPrinter>
#include <cups/cups.h>

#include "print.h"
#include "svgout.h"
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
	/* XFIGQT_NO_GHOSTSCRIPT: try how it is without */
	static QString gs = qEnvironmentVariableIsSet("XFIGQT_NO_GHOSTSCRIPT") ?
			QString() : find_tool({"gs"});
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

/* without Ghostscript: fig2dev's SVG, drawn by Qt on the printer */
static bool
print_svg(QPrinter &printer, QString *err)
{
	char svg[PATH_MAX];
	xq_svg_job job;
	if (xqb_print_svg(svg, &job) != 0)
		return false;
	bool ok = svg_print(job, printer, err);
	QFile::remove(QFile::decodeName(svg));
	return ok;
}

bool
print_figure_with(QPrinter &printer, QWidget *parent)
{
	if (find_ghostscript().isEmpty()) {
		QString err;
		bool pdf = printer.outputFormat() == QPrinter::PdfFormat &&
				!printer.outputFileName().isEmpty();
		bool ok = print_svg(printer, &err);
		if (ok)
			xqb_message((pdf ? QString("Saved \"%1\" as %2").arg(
					figure_title(), printer.outputFileName()) :
				QString("Sent \"%1\" to printer \"%2\"").arg(
					figure_title(), printer.printerName()))
				.toLocal8Bit().constData());
		else if (!err.isEmpty() && parent)
			QMessageBox::warning(parent, "Print", err);
		else if (!err.isEmpty())
			qWarning("%s", qPrintable(err));
		return ok;
	}

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
		QString pdf = tmp.filePath("figure.pdf");
		ok = ps_to_pdf(ps, pdf, &err) && pdf_to_cups(pdf, printer, &err);
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
