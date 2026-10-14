/*
 * svgout.h - PDF, bitmaps and printing without Ghostscript.
 *
 * fig2dev makes PDF and bitmaps from its PostScript with Ghostscript.
 * Without Ghostscript, fig2dev's SVG output is drawn with QtSvg instead.
 * The SVG and the PostScript of fig2dev do not quite agree, so this is
 * only the fallback.
 */
#ifndef SVGOUT_H
#define SVGOUT_H

#ifdef __cplusplus
extern "C" {
#endif

struct xq_svg_job {
	const char	*svg;		/* fig2dev's SVG */
	const char	*dir;		/* picture paths are relative to it */
	const char	*out;		/* the file to write */
	const char	*format;	/* "pdf", or a bitmap: "png", "jpeg"... */
	int		border;		/* margin: points (pdf), or pixels at
					   magnification 1 (bitmaps) */
	double		mag;		/* magnification, 1 = 100% */
	const char	*background;	/* "#rrggbb", or "" for none */
	int		page;		/* place the figure on the paper */
	double		paper_wd, paper_ht;	/* in points, portrait */
	int		landscape;
	int		flushleft;	/* or centered */
	int		smooth;		/* anti-aliased bitmaps */
	int		quality;	/* JPEG quality */
};

/* write job->out; 0 on success, else -1 and xq_svg_error() says why */
int		xq_svg_export(const struct xq_svg_job *job);
const char	*xq_svg_error(void);

/* the export languages drawn from SVG */
int		xq_svg_can_export(const char *lang);

/* render the first page of a PDF file to w x h RGB triples (white
   background); 0 on success, -1 on failure or without QtPdf */
int		xq_pdf_raster(const char *file, int w, int h, unsigned char *rgb);

#ifdef __cplusplus
}

class QPrinter;
class QString;

/* print the job's SVG (job->out unused) on printer */
bool	svg_print(const struct xq_svg_job &job, QPrinter &printer, QString *err);
#endif

#endif
