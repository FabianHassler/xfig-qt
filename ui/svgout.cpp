/*
 * svgout.cpp - PDF, bitmaps and printing without Ghostscript.
 *
 * fig2dev's SVG refers to pictures by file name, which QtSvg does not
 * load: they are put into the SVG as PNG data.  PDF pictures are rendered
 * with QtPdf, if there; EPS pictures need Ghostscript and stay out.
 * Sizes follow fig2dev: bitmaps have 80 pixels per inch, the margin is in
 * points for PDF and in pixels times the magnification for bitmaps.
 */
#include <QtPrintSupport/QPrinter>
#include <QtSvg/QSvgRenderer>
#include <QtGui/QPdfWriter>
#ifdef XQ_HAVE_QTPDF
#include <QtPdf/QPdfDocument>
#endif

#include "svgout.h"
#include "xq.h"
#undef GrayScale

extern "C" void	file_msg(char *format, ...);

static QByteArray	last_error;

const char *
xq_svg_error(void)
{
	return last_error.constData();
}

static int
fail(const QString &msg)
{
	last_error = msg.toLocal8Bit();
	return -1;
}

int
xq_svg_can_export(const char *lang)
{
	static const char *const langs[] = {
		"pdf", "jpeg", "png", "ppm", "tiff", "xbm", "xpm", nullptr
	};
	for (const char *const *l = langs; *l; ++l)
		if (!strcmp(lang, *l))
			return 1;
	return 0;
}

#ifdef XQ_HAVE_QTPDF
static QImage
pdf_image(const QString &file, QSize size)
{
	QPdfDocument doc;
	doc.load(file);
	if (doc.status() != QPdfDocument::Status::Ready || doc.pageCount() < 1)
		return QImage();
	if (!size.isValid())	/* 300 pixels per inch */
		size = (doc.pagePointSize(0) * 300 / 72).toSize();
	QImage img = doc.render(0, size);
	if (img.isNull())
		return img;
	QImage out(img.size(), QImage::Format_RGB32);
	out.fill(Qt::white);
	QPainter p(&out);
	p.drawImage(0, 0, img);
	return out;
}
#endif

int
xq_pdf_raster(const char *file, int w, int h, unsigned char *rgb)
{
#ifdef XQ_HAVE_QTPDF
	QImage img = pdf_image(QFile::decodeName(file), QSize(w, h));
	if (img.isNull())
		return -1;
	for (int y = 0; y < h; ++y) {
		const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
		for (int x = 0; x < w; ++x) {
			*rgb++ = qRed(line[x]);
			*rgb++ = qGreen(line[x]);
			*rgb++ = qBlue(line[x]);
		}
	}
	return 0;
#else
	(void)file; (void)w; (void)h; (void)rgb;
	return -1;
#endif
}

/* a picture as a data: URL; empty if it cannot be read */
static QString
picture_data(const QString &file)
{
	QImage img;
	QFile f(file);
	if (f.open(QIODevice::ReadOnly) && f.peek(5) == "%PDF-") {
#ifdef XQ_HAVE_QTPDF
		img = pdf_image(file, QSize());
#endif
	} else {
		img.load(file);
	}
	if (img.isNull())
		return QString();
	/* the pixels only: a GIF comment, say, is no valid PNG text key */
	img = img.convertToFormat(QImage::Format_ARGB32);
	img = QImage(img.constBits(), img.width(), img.height(),
			img.bytesPerLine(), img.format()).copy();
	QByteArray png;
	QBuffer buf(&png);
	buf.open(QIODevice::WriteOnly);
	img.save(&buf, "PNG");
	return "data:image/png;base64," + QString::fromLatin1(png.toBase64());
}

/* load fig2dev's SVG, with its pictures; size: the figure in points */
static bool
load_svg(const xq_svg_job &job, QSvgRenderer &r, QSizeF *size, QString *err)
{
	QFile f(QFile::decodeName(job.svg));
	if (!f.open(QIODevice::ReadOnly)) {
		*err = QString("Cannot read %1").arg(f.fileName());
		return false;
	}
	QString svg = QString::fromUtf8(f.readAll());
	QDir dir(QFile::decodeName(job.dir));

	static const QRegularExpression href("xlink:href=\"file:([^\"]*)\"");
	QStringList missing;
	QString out;
	qsizetype at = 0;
	for (auto m = href.globalMatch(svg); m.hasNext(); ) {
		auto match = m.next();
		QString name = match.captured(1);
		QString data = picture_data(dir.absoluteFilePath(name));
		if (data.isEmpty())
			missing << QFileInfo(name).fileName();
		out += svg.mid(at, match.capturedStart() - at);
		out += QString("xlink:href=\"%1\"").arg(data);
		at = match.capturedEnd();
	}
	out += svg.mid(at);
	if (!missing.isEmpty())
		file_msg((char *)"Without Ghostscript, these pictures are left "
				"out: %s", missing.join(", ").toLocal8Bit().constData());

	static const QRegularExpression dims(
		"<svg[^>]*width=\"([0-9.]+)pt\"[^>]*height=\"([0-9.]+)pt\"");
	auto d = dims.match(out);
	if (!d.hasMatch() || !r.load(out.toUtf8())) {
		*err = "fig2dev's SVG output cannot be read.";
		return false;
	}
	*size = QSizeF(d.captured(1).toDouble(), d.captured(2).toDouble());
	return true;
}

/* the page in points, and where the figure goes on it */
static void
page_layout(const xq_svg_job &job, QSizeF fig, QSizeF *page, QRectF *where)
{
	if (job.page) {
		*page = job.landscape ? QSizeF(job.paper_ht, job.paper_wd) :
				QSizeF(job.paper_wd, job.paper_ht);
		QPointF at(0, 0);
		if (!job.flushleft)
			at = QPointF((page->width() - fig.width()) / 2,
					(page->height() - fig.height()) / 2);
		*where = QRectF(at, fig);
	} else {
		*page = fig + QSizeF(2 * job.border, 2 * job.border);
		*where = QRectF(QPointF(job.border, job.border), fig);
	}
}

static void
paint(const xq_svg_job &job, QSvgRenderer &r, QPainter &p, QSizeF page,
		QRectF where)
{
	QColor bg(QString::fromLatin1(job.background));
	if (bg.isValid() && bg != Qt::white)
		p.fillRect(job.page ? where : QRectF(QPointF(0, 0), page), bg);
	r.render(&p, where);
}

static int
export_pdf(const xq_svg_job &job, QSvgRenderer &r, QSizeF fig)
{
	QSizeF page;
	QRectF where;
	page_layout(job, fig, &page, &where);

	QPdfWriter pdf(QFile::decodeName(job.out));
	pdf.setCreator("xfig");
	pdf.setPageLayout(QPageLayout(QPageSize(page, QPageSize::Point),
				QPageLayout::Portrait, QMarginsF()));
	pdf.setResolution(1200);
	QPainter p;
	if (!p.begin(&pdf))
		return fail(QString("Cannot write %1").arg(QFile::decodeName(job.out)));
	p.scale(1200.0 / 72, 1200.0 / 72);
	paint(job, r, p, page, where);
	p.end();
	return 0;
}

static int
export_bitmap(const xq_svg_job &job, QSvgRenderer &r, QSizeF fig)
{
	const double ppi = 80;
	int border = qRound(job.border * job.mag);
	QSize size = (fig * ppi / 72).toSize() + QSize(2 * border, 2 * border);
	QImage img(size, QImage::Format_RGB32);
	QColor bg(QString::fromLatin1(job.background));
	img.fill(bg.isValid() ? bg : QColor(Qt::white));
	QPainter p(&img);
	p.setRenderHint(QPainter::Antialiasing, job.smooth);
	p.setRenderHint(QPainter::TextAntialiasing, job.smooth);
	p.setRenderHint(QPainter::SmoothPixmapTransform, job.smooth);
	r.render(&p, QRectF(QPointF(border, border), fig * ppi / 72));
	p.end();

	QByteArray format = QByteArray(job.format) == "jpeg" ? "jpg" : job.format;
	if (!img.save(QFile::decodeName(job.out), format.constData(),
				format == "jpg" ? job.quality : -1))
		return fail(QString("Cannot write %1 as %2").arg(
				QFile::decodeName(job.out), job.format));
	return 0;
}

int
xq_svg_export(const xq_svg_job *job)
{
	QSvgRenderer r;
	QSizeF fig;
	QString err;
	if (!load_svg(*job, r, &fig, &err))
		return fail(err);
	if (!strcmp(job->format, "pdf"))
		return export_pdf(*job, r, fig);
	return export_bitmap(*job, r, fig);
}

bool
svg_print(const xq_svg_job &job, QPrinter &printer, QString *err)
{
	QSvgRenderer r;
	QSizeF fig, page;
	QRectF where;
	if (!load_svg(job, r, &fig, err))
		return false;
	page_layout(job, fig, &page, &where);

	printer.setFullPage(true);
	printer.setPageLayout(QPageLayout(
			QPageSize(QSizeF(job.paper_wd, job.paper_ht), QPageSize::Point),
			job.landscape ? QPageLayout::Landscape : QPageLayout::Portrait,
			QMarginsF()));
	QPainter p;
	if (!p.begin(&printer)) {
		*err = "Cannot print.";
		return false;
	}
	double s = printer.resolution() / 72.0;
	p.scale(s, s);
	p.setRenderHint(QPainter::Antialiasing);
	paint(job, r, p, page, where);
	p.end();
	return true;
}
