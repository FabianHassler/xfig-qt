/*
 * print.cpp - print through the macOS print dialog.
 *
 * xfig prints by piping fig2dev's PostScript into lp or lpr, with a typed
 * printer name.  Here xfig writes the same PostScript (its print settings:
 * magnification, paper, orientation, layers) to a file, the print dialog
 * chooses printer, copies and pages.  Ghostscript's ps2pdf makes a PDF of
 * it, which goes to CUPS, the macOS print system (it no longer takes
 * PostScript), or to the file "Save as PDF" chose.  It stays vector.
 */
/* before xq.h: the X11 shim defines GrayScale, a QPrinter enum value */
#include <QtPrintSupport/QPrintDialog>
#include <QtPrintSupport/QPrinter>
#include <cups/cups.h>

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
find_tool(const QString &name)
{
	QString p = QStandardPaths::findExecutable(name);
	if (p.isEmpty())
		p = QStandardPaths::findExecutable(name,
				{"/opt/homebrew/bin", "/usr/local/bin"});
	return p;
}

static bool
ps_to_pdf(const QString &ps, const QString &pdf, QString *err)
{
	QString ps2pdf = find_tool("ps2pdf");
	if (ps2pdf.isEmpty()) {
		*err = "ps2pdf (Ghostscript) was not found.";
		return false;
	}
	QProcess p;
	p.start(ps2pdf, {ps, pdf});
	if (!p.waitForFinished(120000) || p.exitStatus() != QProcess::NormalExit ||
			p.exitCode() != 0) {
		*err = QString("ps2pdf failed: %1").arg(
				QString::fromLocal8Bit(p.readAllStandardError()).trimmed());
		return false;
	}
	return true;
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

bool
print_figure_with(const QPrinter &printer, QWidget *parent)
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
