/*
 * FIG : Facility for Interactive Generation of figures
 * Copyright (c) 1985-1988 by Supoj Sutanthavibul
 * Parts Copyright (c) 1989-2015 by Brian V. Smith
 * Parts Copyright (c) 1991 by Paul King
 * Parts Copyright (c) 2016-2024 by Thomas Loimer
 *
 * Any party obtaining a copy of these files is granted, free of charge, a
 * full and unrestricted irrevocable, world-wide, paid up, royalty-free,
 * nonexclusive right and license to deal in this software and documentation
 * files (the "Software"), including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense and/or sell copies of
 * the Software, and to permit persons who receive copies from any such
 * party to do so, with the only requirement being that the above copyright
 * and this permission notice remain intact.
 *
 */

#include "u_fonts.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <math.h>
#include <fontconfig/fontconfig.h>	/* FcNameUnparse() */
#include <X11/Xft/Xft.h>

#include "resources.h"
#include "object.h"
#include "w_msgpanel.h"		/* file_msg() */
#include "w_setup.h"		/* DISPLAY_PIX_PER_INCH */
#include "w_zoom.h"		/* zoomscale */

#define DEF_PS_FONT		0
#define FONTS_KEEPOPEN		16
/* to determine the maximum ascent and descent of a font */
#define MAX_HEIGHT_STR		"¿¡{|(¸fgjÅÈ"

/* PostScript font names corresponding to fonts requested in xft_name[] */

struct _fstruct ps_fontinfo[NUM_FONTS + 1] = {
	{"Default",				-1},
	{"Times-Roman",				0},
	{"Times-Italic",			1},
	{"Times-Bold",				2},
	{"Times-BoldItalic",			3},
	{"AvantGarde-Book",			4},
	{"AvantGarde-BookOblique",		5},
	{"AvantGarde-Demi",			6},
	{"AvantGarde-DemiOblique",		7},
	{"Bookman-Light",			8},
	{"Bookman-LightItalic",			9},
	{"Bookman-Demi",			10},
	{"Bookman-DemiItalic",			11},
	{"Courier",				12},
	{"Courier-Oblique",			13},
	{"Courier-Bold",			14},
	{"Courier-BoldOblique",			15},
	{"Helvetica",				16},
	{"Helvetica-Oblique",			17},
	{"Helvetica-Bold",			18},
	{"Helvetica-BoldOblique",		19},
	{"Helvetica-Narrow",			20},
	{"Helvetica-Narrow-Oblique",		21},
	{"Helvetica-Narrow-Bold",		22},
	{"Helvetica-Narrow-BoldOblique",	23},
	{"NewCenturySchlbk-Roman",		24},
	{"NewCenturySchlbk-Italic",		25},
	{"NewCenturySchlbk-Bold",		26},
	{"NewCenturySchlbk-BoldItalic",		27},
	{"Palatino-Roman",			28},
	{"Palatino-Italic",			29},
	{"Palatino-Bold",			30},
	{"Palatino-BoldItalic",			31},
	{"Symbol",				32},
	{"ZapfChancery-MediumItalic",		33},
	{"ZapfDingbats",			34},
};
#define FONT_SYMBOL	32
#define FONT_DINGBATS	34

/*
 * Xft font names
 * These are really the free-form fontconfig patterns.
 * The "Adobe..."-names usually select an X bitmap font.
 * Fontconfig seems to ignore space or uppercase. Nevertheless, for
 * readability, some uppercase letters are retained.
 * Microsoft names, viz., Times New Roman, Courier New, Arial, are
 * probably selected by fontconfig, thus do not mention them explicitly.
 */
const char *const xft_name[NUM_FONTS] = {
	"times,AdobeTimes,serif:roman",		/* Times-Roman */
	"times,AdobeTimes,serif:italic",	/* Times-Italic */
	"times,AdobeTimes,serif:bold",		/* Times-Bold */
	"times,AdobeTimes,serif:bold:italic",	/* Times-BoldItalic */
	"avantgarde:book",			/* AvantGarde-Book */
	"avantgarde:book:oblique",		/* AvantGarde-BookOblique */
	"avantgarde:demibold",			/* AvantGarde-Demi */
	"avantgarde:demibold:oblique",		/* AvantGarde-DemiOblique */
	"bookman:light",			/* Bookman-Light */
	"bookman:light:italic",			/* Bookman-LightItalic */
	"bookman:demibold",			/* Bookman-Demi */
	"bookman:demibold:italic",		/* Bookman-DemiItalic */
	"courier,AdobeCourier,monospace",	/* Courier */
	"courier,AdobeCourier,monospace:oblique",	/* Courier-Oblique */
	"courier,AdobeCourier,monospace:bold",		/* Courier-Bold */
	"courier,AdobeCourier,monospace:bold:oblique",	/* Courier-BoldOblique*/
	"helvetica,sans",			/* Helvetica */
	"helvetica,sans:oblique",		/* Helvetica-Oblique */
	"helvetica,sans:bold",			/* Helvetica-Bold */
	"helvetica,sans:bold:oblique",		/* Helvetica-BoldOblique */
	"helveticanarrow,sans:semicondensed",	/* Helvetica-Narrow */
	"helveticanarrow,sans:semicondensed:oblique",
					/* Helvetica-Narrow-Oblique */
	"helveticanarrow,sans:semicondensed:bold",
					/* Helvetica-Narrow-Bold */
	"helveticanarrow,sans:semicondensed:bold:oblique",
					/* Helvetica-Narrow-BoldOblique */
	"newcenturyschoolbook,AdobeNewCenturySchoolbook",
					/* NewCenturySchlbk-Roman */
	"newcenturyschoolbook,AdobeNewCenturySchoolbook:italic",
					/* NewCenturySchlbk-Italic */
	"newcenturyschoolbook,AdobeNewCenturySchoolbook:bold",
					/* NewCenturySchlbk-Bold */
	"newcenturyschoolbook,AdobeNewCenturySchoolbook:bold:italic",
					/* NewCenturySchlbk-BoldItalic */
	"palatino",				/* Palatino-Roman */
	"palatino:italic",			/* Palatino-Italic */
	"palatino:bold",			/* Palatino-Bold */
	"palatino:bold:italic",			/* Palatino-BoldItalic */
	"symbol,StandardSymbolsPS,StandardSymbolsL:fontformat=CFF,TrueType,Type 1",
	"zapfchancery:medium:italic",		/* ZapfChancery-MediumItalic */
	"zapfdingbats,ITCZapfDingbats,Dingbats,D050000L:fontformat=CFF,TrueType,Type 1"
};


/* LaTeX font names and the corresponding PostScript font index into ps_fontinfo */

struct _fstruct latex_fontinfo[NUM_LATEX_FONTS] = {
	{"Default",		0},
	{"Roman",		0},
	{"Bold",		2},
	{"Italic",		1},
	{"Sans Serif",		16},
	{"Typewriter",		12},
};

/*
 * Font storage, reference-counted.
 * The id is a fixed-point combination of pixelsize and angle.
 * Pixelsize overrides size and different sizes do not return different font
 * faces, hence a distinction on size is unnecessary.
 * id (32 bit):   |    pixelsize (13.4)   |   angle (3.12)   |
 * angle - fixed 3.12, 2^12 = 4096, resolution = 1 / 4096 rad = 0.014°
 * size - fixed 13.4, resolved to 1/16 points; max size = 500, with zoom = 128
 *	the pixelsize becomes approx. 512 * 128 = 2^(9+7) = 2^16. Since the
 *	possible pixelsize might be exceeded, do not store fonts with invalid id
 *	in font_storage.
 */
static struct font_storage {
	XFIG_FONT;
	unsigned int		ref;
	struct font_storage	*next;
} *font_storage[NUM_FONTS];

#define	ANGLE_FRACTIONAL	12
#define ANGLE_WIDTH		15
#define	ANGLE_MASK		((1 << ANGLE_WIDTH) - 1)
#define PIXELSIZE_FRACTIONAL	 4
#define	NOTSTORED		(7 << ANGLE_FRACTIONAL)

/*
 * Declaration of _charmap(int, XftFont *) returning a function pointer
 * to XftChar32 (*)(XftChar8).
 */
static XftChar32	(* _charmap(int fnum, XftFont *xft_font))(XftChar8);

static int
fontnum(int psflag, int fnum)
{
	if (fnum < 0 || (psflag && fnum >= NUM_FONTS) ||
			(!psflag && fnum >= NUM_LATEX_FONTS)) {
		file_msg("Illegal font number, using default font.");
		fnum = DEF_PS_FONT;
	}
	if (!psflag)
		fnum = latex_fontinfo[fnum].xfontnum;
	return fnum;
}

int
psfontnum(char *font)
{
	int	i;

	if (font == NULL)
		return(DEF_PS_FONT);
	for (i = 0; i < NUM_FONTS; ++i)	/* Do not start with Zapf Dingbats */
		if (strcasecmp(ps_fontinfo[i].name, font) == 0)
			return i - 1;
	return(DEF_PS_FONT);
}

int latexfontnum(char *font)
{
	int	i;

	if (font == NULL)
		return DEF_LATEX_FONT;
	for (i = 0; i < NUM_LATEX_FONTS; i++)
		if (strcasecmp(latex_fontinfo[i].name, font) == 0)
			return i;
	return(DEF_LATEX_FONT);
}

void
font_close(struct xfig_font *xf_font)
{
	int			fnum;
	static int		called = 0;
	struct font_storage	*fonts = (struct font_storage *)xf_font;

	if (xf_font->id == NOTSTORED) {
		XftFontClose(tool_d, xf_font->xft_font);
		free(xf_font);
		return;
	}

	++called;
	--fonts->ref;

	if (called < 64)
		return;

	called = 0;

	/* every 64 times called, free all but the last 16 unused variants
	   of each font */
	for (fnum = 0; fnum < NUM_FONTS; ++fnum) {
		int			i = FONTS_KEEPOPEN;

		fonts = font_storage[fnum];
		if (!fonts)
			continue;

		while (fonts->next) {
			if (fonts->next->ref == 0) {
				if (i != 0) {
					--i;
				} else {
					struct font_storage *del = fonts->next;
					fonts->next = del->next;
					XftFontClose(tool_d, del->xft_font);
					free(del);
					if (!fonts->next)
						break;
				}
			}
			fonts = fonts->next;
		}
	}
}

#ifdef FREEMEM
/*
 * Free the entire font storage. This is called before exit of the program,
 * hence do not care about setting remaining invalid pointers to NULL.
 */
void
font_closeall(void)
{
	int			fnum;
	struct font_storage	*fonts;
	struct font_storage	*del;

	for (fnum = 0; fnum < NUM_FONTS; ++fnum) {
		fonts = font_storage[fnum];
		while (fonts) {
			del = fonts;
			fonts = fonts->next;
			XftFontClose(tool_d, del->xft_font);
			free(del);
		}
	}
}
#endif /* FREEMEM */

/*
 * Bypass font-storage. Was used for MONO_FONT.
 */
/*
XftFont *
font_opencustom(const char *want)
{
	XftPattern	*pattern;
	XftPattern	*match;
	XftResult	result;
	double		dbl;

	pattern = XftNameParse(want);
	if (XftResultMatch != XftPatternGetDouble(pattern, XFT_SIZE, 0, &dbl)) {
		dbl = 10.;
		file_msg("Setting size for requested font '%s' to %.1f",
				want, dbl);
	}
	dbl *= DISPLAY_PIX_PER_INCH / (appres.correct_font_size ? 72.0 : 80.0);
	match = XftFontMatch(tool_d, tool_sn, pattern, &result);
	XftPatternDestroy(pattern);
	return XftFontOpenPattern(tool_d, match);
}
*/

struct xfig_font *
font_open(int psflag, int fnum, double size, double angle)
{
	unsigned long		id;
	unsigned long		n;
	const double		mult_size = 1 << PIXELSIZE_FRACTIONAL;
	const double		mult_angle = 1 << ANGLE_FRACTIONAL;
	double			pixelsize;
	XftResult		result;
	XftPattern		*pattern;
	XftPattern		*match;
	struct font_storage	*fonts;

	/* sanitize fnum */
	fnum = fontnum(psflag, fnum);

	pixelsize = size * DISPLAY_PIX_PER_INCH /
				(appres.correct_font_size ? 72.0 : 80.0);

	/* For too large pixelsize, return a unique font struct. */
	if (pixelsize > (UINT_MAX >> (ANGLE_WIDTH + PIXELSIZE_FRACTIONAL))) {
		/*
		 * But if the pixelsize does not fit into 12.4, allocate
		 * and return an unique font struct.
		 */
		struct xfig_font	*new;

		id = NOTSTORED;

		/* get the pattern */
		if (font_storage[fnum]) {
			/* either a suitable pattern exists, modify it */
			pattern = XftPatternDuplicate(
					font_storage[fnum]->xft_font->pattern);
			XftPatternDel(pattern, XFT_MATRIX);
			XftPatternDel(pattern, XFT_PIXEL_SIZE);
		} else {
			/* create the base pattern, see below */
			pattern = XftNameParse(xft_name[fnum]);
			XftPatternAddBool(pattern, XFT_SCALABLE, True);
		}

		/* add the correct pattern specifications */
		XftPatternAddDouble(pattern, XFT_PIXEL_SIZE, pixelsize);
		if (angle != 0.0) {
			double		cosa = cos(angle);
			double		sina = sin(angle);
			XftMatrix	mat = {cosa, -sina, sina, cosa};
			XftPatternAddMatrix(pattern, XFT_MATRIX, &mat);
		}

		/* and get the font */
		match = XftFontMatch(tool_d, tool_sn, pattern, &result);
		XftPatternDestroy(pattern);

		new = malloc(sizeof(struct xfig_font));
		new->id = id;
		new->xft_font = XftFontOpenPattern(tool_d, match);
		return new;
	}
	/* Compute the identifier for this pixelsize and angle combination. */
	id = (unsigned long)(angle * mult_angle + 0.5);
	angle = id / mult_angle;

	n = (unsigned long)(pixelsize * mult_size + 0.5);
	pixelsize = n / mult_size;

	id += n << ANGLE_WIDTH;

	/* search, whether this font already has been opened */
	fonts = font_storage[fnum];
	if (fonts) {
		while (fonts) {
			if (fonts->id == id) {
				++fonts->ref;
				return (struct xfig_font *)fonts;
			}
			fonts = fonts->next;
		}
		/* not found, get the base pattern */
		fonts = font_storage[fnum];
		pattern = XftPatternDuplicate(fonts->xft_font->pattern);

		/* delete incorrect values */
		if ((id & ANGLE_MASK) != (fonts->id & ANGLE_MASK))
			XftPatternDel(pattern, XFT_MATRIX);
		if ((id & ~ANGLE_MASK)!=(fonts->id & ~ANGLE_MASK)) {
			XftPatternDel(pattern, XFT_PIXEL_SIZE);
		}
	} else {
		/* nothing here, create the base pattern */
		pattern = XftNameParse(xft_name[fnum]);
		/* Without scalable, a Wolfram pixel font was found for
		   "helvetica". */
		XftPatternAddBool(pattern, XFT_SCALABLE, True);
		/* XftPatternAddBool returns 1, if succesful */
	}

	/* modify the pattern */
	if (!fonts || (id & ~ANGLE_MASK) != (fonts->id & ~ANGLE_MASK))
		XftPatternAddDouble(pattern, XFT_PIXEL_SIZE, pixelsize);

	if (((id & ANGLE_MASK) != 0 ) && (!fonts ||
				(id & ANGLE_MASK) != (fonts->id & ANGLE_MASK))){
		double		cosa = cos(angle);
		double		sina = sin(angle);
		XftMatrix	mat = {cosa, -sina, sina, cosa};
		XftPatternAddMatrix(pattern, XFT_MATRIX, &mat);
	}

	/* allocate and prepend the new font storage element*/
	fonts = malloc(sizeof(struct font_storage));
	if (font_storage[fnum])
		fonts->next = font_storage[fnum];
	else
		fonts->next = NULL;
	font_storage[fnum] = fonts;

	match = XftFontMatch(tool_d, tool_sn, pattern, &result);
	XftPatternDestroy(pattern);
	/* antialias, hintstyle and rgba keep appending */
	FcPatternRemove(match, XFT_ANTIALIAS, 1);
	FcPatternRemove(match, "hintstyle", 1);
	FcPatternRemove(match, XFT_RGBA, 1);

	/* populate the new font structure */
	fonts->ascent = 0u;
	fonts->id = id;
	fonts->ref = 1;
	fonts->xft_font = XftFontOpenPattern(tool_d, match);

	return (struct xfig_font *)fonts;
}

struct xfig_font *
font_xfcopy(struct xfig_font *xf_font)
{
	struct font_storage	*fonts;

	if (xf_font->id == NOTSTORED) {
		struct xfig_font	*new;
		new = malloc(sizeof(struct xfig_font));
		new->id = NOTSTORED;
		new->xft_font = XftFontCopy(tool_d, xf_font->xft_font);
		return new;
	}

	fonts = (struct font_storage *)xf_font;
	++fonts->ref;
	return xf_font;
}

static void
_textextents(XftFont *xft_font, int fnum, const char *str, size_t len,
		XGlyphInfo *extents)
{
	XftChar32	(*map)(XftChar8);

	if ((fnum != FONT_SYMBOL && fnum != FONT_DINGBATS) ||
			(map = _charmap(fnum, xft_font)) == NULL) {
		XftTextExtentsUtf8(tool_d, xft_font, (XftChar8 *)str, len,
				extents);
	} else { /* map != NULL */
		XftChar32	glyphs[len];
		XftChar8	*s = (XftChar8 *)str;
		XftChar8	*chr;
		int		glen = 0;

		for (chr = s; chr < s + len; ++chr) {
			XftChar32	glyph;
			glyph = XftCharIndex(tool_d, xft_font, map(*chr));
			if (glyph)
				glyphs[glen++] = glyph;
		}
		XftGlyphExtents(tool_d, xft_font, glyphs, glen, extents);
	}
}

/*
 * A text object must provide
 *  (i) the total bounding box, to determine wheter a given text object falls
 *  onto the canvas and must be drawn,
 *  (ii) the rotated bounding box, to determine the clickable area when
 *  searching for an object, but also for snapping to endpoints,
 *  (iii) the ascent and descent of the font, for drawing the cursor when
 *  editing the text, and
 *  (iv) the text height and length, format 3.2 requires these to be written to
 *  the .fig file.
 *     _____
 *    |  /t/|
 *    | /x/ |
 *    |/e/--|----- rotated
 *    't/   |----- total bounding box: F_text t->bb[]
 *    `-----'
 *
 * All these informations require possible extensive actions, since
 * XftTextExtents() must be called. Hence, try to evaluate lazily.
 * (i) The bounding box is determined by drawing the tex at rather coarse
 * (canvas) resolution, with canvas_zoomscale = 1. Fewer pixels make probably
 * faster evaluation. At higher resolution, the bounding box is probably a bit
 * smaller than at coarse resolution. Once determined, the bounding box does not
 * change with different zoomscales. Determined by font_setbbox().
 * (ii) Xft draws text at slightly different angles, depending on the resolution
 * (See ticket #169 <https://sourceforge.net/p/mcj/tickets/>. For instance, text
 * at nominally 30° might be drawn at angles between 29.9° and 34.6°. Hence, the
 * rotated bounding box must updated for each zoom scale. The rotated bounding
 * box is defined by the vector along the baseline of the text (t->offset) and
 * the font ascent, hence t->offset is to be updated at each zoom scale.
 * Done with font_textoffset(), also font_textlen(), and font_setbbox_offset().
 * To snap to endpoints, the precise, possibly oblique bounding box must be
 * known, which is given by font_baselinebbox().
 * (iii) Font ascent and descent are properties of the font, at a given size and
 * independent of the angle, hence it is stored with the struct xfig_font. Set
 * by font_ascentdescent().
 *
 */

F_pos
font_textoffset(struct xfig_font *xf_font, int fnum, const char *str, int len)
{
	F_pos	off = font_textlen(xf_font, fnum, str, len);
	off.x = lroundf(off.x / zoomscale);
	off.y = lroundf(off.y / zoomscale);
	return off;
}

/*
 * Compute text->bb[], text->offset, text->length and text->asc_len.
 * Measure these values at rather coarse resolution, at display_zoomscale = 1.
 */
void
font_setbbox_offset(F_text *t)
{
	int			asc, desc;
	int			draw_x, draw_y;
	size_t			len;
	double			length;
	XGlyphInfo		extents;
	struct xfig_font	*xf_font;

	len = strlen(t->cstring);

	/* shortcut, nothing to do for an empty string */
	if (len == 0) {
		t->length = 0;
		t->bb[0].x = t->bb[1].x = t->base_x;
		t->bb[0].y = t->bb[1].y = t->base_y;
		t->offset = (F_pos){0, 0};
		return;
	}

	xf_font = font_open(psfont_text(t), t->font, t->size, t->angle);
	_textextents(xf_font->xft_font, t->font, t->cstring, len, &extents);
	font_close(xf_font);

	t->offset.x = lroundf(extents.xOff * ZOOM_FACTOR);
	t->offset.y = lroundf(extents.yOff * ZOOM_FACTOR);
	text_origin(&draw_x, &draw_y, t->base_x, t->base_y, t->type, t->offset);

	t->bb[0].x = draw_x - (int)ceil(extents.x * ZOOM_FACTOR);
	t->bb[0].y = draw_y - (int)ceil(extents.y * ZOOM_FACTOR);
	t->bb[1].x = draw_x + (int)ceil(
				(extents.width - extents.x) * ZOOM_FACTOR);
	t->bb[1].y = draw_y + (int)ceil(
				(extents.height - extents.y) * ZOOM_FACTOR);

	if (extents.yOff == 0) {
		t->length = abs(t->offset.x);
		length = t->length;
	} else if (extents.xOff == 0) {
		t->length = abs(t->offset.y);
		length = t->length;
	} else {
		length = sqrt(t->offset.x * t->offset.x +
				t->offset.y * t->offset.y);
		t->length = length + 0.5;
	}

	font_ascentdescent(psfont_text(t), t->font, t->size, &asc, &desc);
	/* asc_len is used to determine the rotated bounding box.
	   It is not necessary to be overly precise here. */
	 t->asc_len = asc / length;

	 /* Return the offset for the current zoomscale */
	 if (display_zoomscale != 1.0f)
		 t->offset = font_textoffset(t->xf_font, t->font, t->cstring,
					 len);
}

/*
 * Return the box along the baseline of the text, struct xfig_font->ascent high.
 */
void
font_baselinebbox(F_text *t, F_line *l)
{
	int			ascent;
	int			draw_x, draw_y;
	double			length;
	XGlyphInfo		extents;
	struct xfig_font	*xf_font;

	/* I think, this never happens, but anyway... */
	if (*t->cstring == '\0') {
		F_point	*p = l->points;
		while (p) {
			p->x = t->base_x;
			p->y = t->base_y;
			p = p->next;
		}
		return;
	}

	/* determine the bounding box at fig-units resolution (1200 dpi) */
	/* first the ascent, if necessary */
	if (t->angle != 0.0) {
		xf_font = font_open(psfont_text(t), t->font,
				t->size * ZOOM_FACTOR, 0.0);
		_textextents(xf_font->xft_font, t->font, MAX_HEIGHT_STR,
				strlen(MAX_HEIGHT_STR), &extents);
		ascent = extents.y;
	}
	/* now the extents of the oblique text */
	xf_font = font_open(psfont_text(t), t->font, t->size * ZOOM_FACTOR,
			t->angle);
	_textextents(xf_font->xft_font, t->font, t->cstring, strlen(t->cstring),
			&extents);
	font_close(xf_font);
	if (t->angle == 0.0)
		ascent = extents.y;

	text_origin(&draw_x, &draw_y, t->base_x, t->base_y, t->type,
			(F_pos){extents.xOff, extents.yOff});
	if (extents.yOff == 0)
		length = abs(extents.xOff);
	else if (extents.xOff == 0)
		length = abs(extents.yOff);
	else
		length = sqrt(extents.xOff * extents.xOff +
				extents.yOff * extents.yOff);
	l->points->x = draw_x;
	l->points->y = draw_y;
	l->points->next->x = draw_x + extents.xOff;
	l->points->next->y = draw_y + extents.yOff;
	l->points->next->next->x = lround(draw_x + extents.xOff +
			extents.yOff * ascent / length);
	l->points->next->next->y = lround(draw_y + extents.yOff -
			extents.xOff * ascent / length);
	l->points->next->next->next->x = lround(draw_x +
			extents.yOff * ascent / length);
	l->points->next->next->next->y = lround(draw_y -
			extents.xOff * ascent / length);
	l->points->next->next->next->next->x = l->points->x;
	l->points->next->next->next->next->y = l->points->y;
}

/*
 * Return a vector representing the length of the first len bytes of string
 * when drawn with xf_font.
 */
F_pos
font_textlen(struct xfig_font *xf_font, int fnum, const char *string, int len)
{
	XGlyphInfo	extents;

	if (len == 0)
		return (F_pos){0, 0};

	_textextents(xf_font->xft_font, fnum, string, len, &extents);
	return (F_pos){extents.xOff, extents.yOff};
}

/*
 * Return ascent and descent, testing with a few chars that should provide a
 * good approximation to the maximum ascent and descent.
 * This works mainly for western languages, not sure about eastern languages.
 */
void
font_ascentdescent(int psflag, int fnum, int size, int *ascent, int *descent)
{
	struct xfig_font	*horfont;

	/* determine ascent and descent at display_zoomscale = 1.0 */
	horfont = font_open(psflag, fnum, size, 0.0);
	if (horfont->ascent == 0u) {
		XGlyphInfo		extents;

		_textextents(horfont->xft_font, fnum, MAX_HEIGHT_STR,
				strlen(MAX_HEIGHT_STR), &extents);
		horfont->ascent = extents.y * ZOOM_FACTOR + 0.5;
		horfont->descent = (extents.height - extents.y) * ZOOM_FACTOR
									+ 0.5;
	}
	*ascent = horfont->ascent;
	*descent = horfont->descent;
	font_close(horfont);
}

/*
 * Return the drawing origin, given the marker position (base_x, base_y)
 * and the alignment.
 */
void
text_origin(int *draw_x, int *draw_y, int base_x, int base_y, int align,
		F_pos offset)
{
	switch (align) {
	case T_LEFT_JUSTIFIED:
		*draw_x = base_x;
		*draw_y = base_y;
		break;
	case T_CENTER_JUSTIFIED:
		*draw_x = base_x - offset.x/2;
		*draw_y = base_y - offset.y/2;
		break;
	case T_RIGHT_JUSTIFIED:
		*draw_x = base_x - offset.x;
		*draw_y = base_y - offset.y;
		break;
	default:
		file_msg("Incorrect text alignment %d, "
				"error in function text_origin().", align);
		break;
	}
}

/*
 * Return the marker position, given the drawing origin.
 * The reverse operation from above.
 */
void
font_base(int *base_x, int *base_y, int draw_x, int draw_y, int align,
		F_pos offset)
{
	switch (align) {
	case T_LEFT_JUSTIFIED:
		*base_x = draw_x;
		*base_y = draw_y;
		break;
	case T_CENTER_JUSTIFIED:
		*base_x = draw_x + offset.x/2;
		*base_y = draw_y + offset.y/2;
		break;
	case T_RIGHT_JUSTIFIED:
		*base_x = draw_x + offset.x;
		*base_y = draw_y + offset.y;
		break;
	default:
		file_msg("Incorrect text alignment %d, "
				"error in function font_base().", align);
		break;
	}
}

void
font_reload(F_text *t)
{
	t->zoom = zoomscale;
	if (t->xf_font)
		font_close(t->xf_font);
	t->xf_font = font_open(psfont_text(t), t->font,
			t->size * display_zoomscale, t->angle);
	t->offset = font_textoffset(t->xf_font, t->font, t->cstring,
			strlen(t->cstring));
}

/*
 * The URW fonts StandardSymbols and Zapf Dingbats have their glyphs stored at
 * positions 32 to 255 in the TrueType and OpenType font files. However, in the
 * Type 1 font files the glyphs are stored at their correct unicode positions.
 * Therefore, if a Symbol or Dingbats font is opened which does not contain,
 * e.g., a lowercase 'a', the positions must be remapped.
 * The mappings are provided by the functions map_dingbats() and map_symbols(),
 * while _charmap() is queried and returns the map function, or NULL.
 */
/*
 * Unicode mapping found in
 * /usr/share/fonts/encodings/adobe-dingbats.enc.gz
 */
XftChar32
map_dingbats(XftChar8 in)
{
	switch (in) {
	case 0x20: return 0x0020;    /* SPACE */
	case 0x21: return 0x2701;    /* UPPER BLADE SCISSORS */
	case 0x22: return 0x2702;    /* BLACK SCISSORS */
	case 0x23: return 0x2703;    /* LOWER BLADE SCISSORS */
	case 0x24: return 0x2704;    /* WHITE SCISSORS */
	case 0x25: return 0x260E;    /* BLACK TELEPHONE */
	case 0x26: return 0x2706;    /* TELEPHONE LOCATION SIGN */
	case 0x27: return 0x2707;    /* TAPE DRIVE */
	case 0x28: return 0x2708;    /* AIRPLANE */
	case 0x29: return 0x2709;    /* ENVELOPE */
	case 0x2A: return 0x261B;    /* BLACK RIGHT POINTING INDEX */
	case 0x2B: return 0x261E;    /* WHITE RIGHT POINTING INDEX */
	case 0x2C: return 0x270C;    /* VICTORY HAND */
	case 0x2D: return 0x270D;    /* WRITING HAND */
	case 0x2E: return 0x270E;    /* LOWER RIGHT PENCIL */
	case 0x2F: return 0x270F;    /* PENCIL */
	case 0x30: return 0x2710;    /* UPPER RIGHT PENCIL */
	case 0x31: return 0x2711;    /* WHITE NIB */
	case 0x32: return 0x2712;    /* BLACK NIB */
	case 0x33: return 0x2713;    /* CHECK MARK */
	case 0x34: return 0x2714;    /* HEAVY CHECK MARK */
	case 0x35: return 0x2715;    /* MULTIPLICATION X */
	case 0x36: return 0x2716;    /* HEAVY MULTIPLICATION X */
	case 0x37: return 0x2717;    /* BALLOT X */
	case 0x38: return 0x2718;    /* HEAVY BALLOT X */
	case 0x39: return 0x2719;    /* OUTLINED GREEK CROSS */
	case 0x3A: return 0x271A;    /* HEAVY GREEK CROSS */
	case 0x3B: return 0x271B;    /* OPEN CENTRE CROSS */
	case 0x3C: return 0x271C;    /* HEAVY OPEN CENTRE CROSS */
	case 0x3D: return 0x271D;    /* LATIN CROSS */
	case 0x3E: return 0x271E;    /* SHADOWED WHITE LATIN CROSS */
	case 0x3F: return 0x271F;    /* OUTLINED LATIN CROSS */
	case 0x40: return 0x2720;    /* MALTESE CROSS */
	case 0x41: return 0x2721;    /* STAR OF DAVID */
	case 0x42: return 0x2722;    /* FOUR TEARDROP-SPOKED ASTERISK */
	case 0x43: return 0x2723;    /* FOUR BALLOON-SPOKED ASTERISK */
	case 0x44: return 0x2724;    /* HEAVY FOUR BALLOON-SPOKED ASTERISK */
	case 0x45: return 0x2725;    /* FOUR CLUB-SPOKED ASTERISK */
	case 0x46: return 0x2726;    /* BLACK FOUR POINTED STAR */
	case 0x47: return 0x2727;    /* WHITE FOUR POINTED STAR */
	case 0x48: return 0x2605;    /* BLACK STAR */
	case 0x49: return 0x2729;    /* STRESS OUTLINED WHITE STAR */
	case 0x4A: return 0x272A;    /* CIRCLED WHITE STAR */
	case 0x4B: return 0x272B;    /* OPEN CENTRE BLACK STAR */
	case 0x4C: return 0x272C;    /* BLACK CENTRE WHITE STAR */
	case 0x4D: return 0x272D;    /* OUTLINED BLACK STAR */
	case 0x4E: return 0x272E;    /* HEAVY OUTLINED BLACK STAR */
	case 0x4F: return 0x272F;    /* PINWHEEL STAR */
	case 0x50: return 0x2730;    /* SHADOWED WHITE STAR */
	case 0x51: return 0x2731;    /* HEAVY ASTERISK */
	case 0x52: return 0x2732;    /* OPEN CENTRE ASTERISK */
	case 0x53: return 0x2733;    /* EIGHT SPOKED ASTERISK */
	case 0x54: return 0x2734;    /* EIGHT POINTED BLACK STAR */
	case 0x55: return 0x2735;    /* EIGHT POINTED PINWHEEL STAR */
	case 0x56: return 0x2736;    /* SIX POINTED BLACK STAR */
	case 0x57: return 0x2737;    /* EIGHT POINTED RECTILINEAR BLACK STAR */
	case 0x58: return 0x2738;    /* HEAVY EIGHT POINTED RECTILINEAR BLACK STAR */
	case 0x59: return 0x2739;    /* TWELVE POINTED BLACK STAR */
	case 0x5A: return 0x273A;    /* SIXTEEN POINTED ASTERISK */
	case 0x5B: return 0x273B;    /* TEARDROP-SPOKED ASTERISK */
	case 0x5C: return 0x273C;    /* OPEN CENTRE TEARDROP-SPOKED ASTERISK */
	case 0x5D: return 0x273D;    /* HEAVY TEARDROP-SPOKED ASTERISK */
	case 0x5E: return 0x273E;    /* SIX PETALLED BLACK AND WHITE FLORETTE */
	case 0x5F: return 0x273F;    /* BLACK FLORETTE */
	case 0x60: return 0x2740;    /* WHITE FLORETTE */
	case 0x61: return 0x2741;    /* EIGHT PETALLED OUTLINED BLACK FLORETTE */
	case 0x62: return 0x2742;    /* CIRCLED OPEN CENTRE EIGHT POINTED STAR */
	case 0x63: return 0x2743;    /* HEAVY TEARDROP-SPOKED PINWHEEL ASTERISK */
	case 0x64: return 0x2744;    /* SNOWFLAKE */
	case 0x65: return 0x2745;    /* TIGHT TRIFOLIATE SNOWFLAKE */
	case 0x66: return 0x2746;    /* HEAVY CHEVRON SNOWFLAKE */
	case 0x67: return 0x2747;    /* SPARKLE */
	case 0x68: return 0x2748;    /* HEAVY SPARKLE */
	case 0x69: return 0x2749;    /* BALLOON-SPOKED ASTERISK */
	case 0x6A: return 0x274A;    /* EIGHT TEARDROP-SPOKED PROPELLER ASTERISK */
	case 0x6B: return 0x274B;    /* HEAVY EIGHT TEARDROP-SPOKED PROPELLER ASTERISK */
	case 0x6C: return 0x25CF;    /* BLACK CIRCLE */
	case 0x6D: return 0x274D;    /* SHADOWED WHITE CIRCLE */
	case 0x6E: return 0x25A0;    /* BLACK SQUARE */
	case 0x6F: return 0x274F;    /* LOWER RIGHT DROP-SHADOWED WHITE SQUARE */
	case 0x70: return 0x2750;    /* UPPER RIGHT DROP-SHADOWED WHITE SQUARE */
	case 0x71: return 0x2751;    /* LOWER RIGHT SHADOWED WHITE SQUARE */
	case 0x72: return 0x2752;    /* UPPER RIGHT SHADOWED WHITE SQUARE */
	case 0x73: return 0x25B2;    /* BLACK UP-POINTING TRIANGLE */
	case 0x74: return 0x25BC;    /* BLACK DOWN-POINTING TRIANGLE */
	case 0x75: return 0x25C6;    /* BLACK DIAMOND */
	case 0x76: return 0x2756;    /* BLACK DIAMOND MINUS WHITE X */
	case 0x77: return 0x25D7;    /* RIGHT HALF BLACK CIRCLE */
	case 0x78: return 0x2758;    /* LIGHT VERTICAL BAR */
	case 0x79: return 0x2759;    /* MEDIUM VERTICAL BAR */
	case 0x7A: return 0x275A;    /* HEAVY VERTICAL BAR */
	case 0x7B: return 0x275B;    /* HEAVY SINGLE TURNED COMMA QUOTATION MARK ORNAMENT */
	case 0x7C: return 0x275C;    /* HEAVY SINGLE COMMA QUOTATION MARK ORNAMENT */
	case 0x7D: return 0x275D;    /* HEAVY DOUBLE TURNED COMMA QUOTATION MARK ORNAMENT */
	case 0x7E: return 0x275E;    /* HEAVY DOUBLE COMMA QUOTATION MARK ORNAMENT */
	/* 0x7F */
	case 0x80: return 0xF8D7;    /* MEDIUM LEFT PARENTHESIS ORNAMENT */
	case 0x81: return 0xF8D8;    /* MEDIUM RIGHT PARENTHESIS ORNAMENT */
	case 0x82: return 0xF8D9;    /* MEDIUM FLATTENED LEFT PARENTHESIS ORNAMENT */
	case 0x83: return 0xF8DA;    /* MEDIUM FLATTENED RIGHT PARENTHESIS ORNAMENT */
	case 0x84: return 0xF8DB;    /* MEDIUM LEFT-POINTING ANGLE BRACKET ORNAMENT */
	case 0x85: return 0xF8DC;    /* MEDIUM RIGHT-POINTING ANGLE BRACKET ORNAMENT */
	case 0x86: return 0xF8DD;    /* HEAVY LEFT-POINTING ANGLE QUOTATION MARK ORNAMENT */
	case 0x87: return 0xF8DE;    /* HEAVY RIGHT-POINTING ANGLE QUOTATION MARK ORNAMENT */
	case 0x88: return 0xF8DF;    /* HEAVY LEFT-POINTING ANGLE BRACKET ORNAMENT */
	case 0x89: return 0xF8E0;    /* HEAVY RIGHT-POINTING ANGLE BRACKET ORNAMENT */
	case 0x8A: return 0xF8E1;    /* LIGHT LEFT TORTOISE SHELL BRACKET ORNAMENT */
	case 0x8B: return 0xF8E2;    /* LIGHT RIGHT TORTOISE SHELL BRACKET ORNAMENT */
	case 0x8C: return 0xF8E3;    /* MEDIUM LEFT CURLY BRACKET ORNAMENT */
	case 0x8D: return 0xF8E4;    /* MEDIUM RIGHT CURLY BRACKET ORNAMENT */
	/* 0x8E - 0xA0 */
	case 0xA1: return 0x2761;    /* CURVED STEM PARAGRAPH SIGN ORNAMENT */
	case 0xA2: return 0x2762;    /* HEAVY EXCLAMATION MARK ORNAMENT */
	case 0xA3: return 0x2763;    /* HEAVY HEART EXCLAMATION MARK ORNAMENT */
	case 0xA4: return 0x2764;    /* HEAVY BLACK HEART */
	case 0xA5: return 0x2765;    /* ROTATED HEAVY BLACK HEART BULLET */
	case 0xA6: return 0x2766;    /* FLORAL HEART */
	case 0xA7: return 0x2767;    /* ROTATED FLORAL HEART BULLET */
	case 0xA8: return 0x2663;    /* BLACK CLUB SUIT */
	case 0xA9: return 0x2666;    /* BLACK DIAMOND SUIT */
	case 0xAA: return 0x2665;    /* BLACK HEART SUIT */
	case 0xAB: return 0x2660;    /* BLACK SPADE SUIT */
	case 0xAC: return 0x2460;    /* CIRCLED DIGIT ONE */
	case 0xAD: return 0x2461;    /* CIRCLED DIGIT TWO */
	case 0xAE: return 0x2462;    /* CIRCLED DIGIT THREE */
	case 0xAF: return 0x2463;    /* CIRCLED DIGIT FOUR */
	case 0xB0: return 0x2464;    /* CIRCLED DIGIT FIVE */
	case 0xB1: return 0x2465;    /* CIRCLED DIGIT SIX */
	case 0xB2: return 0x2466;    /* CIRCLED DIGIT SEVEN */
	case 0xB3: return 0x2467;    /* CIRCLED DIGIT EIGHT */
	case 0xB4: return 0x2468;    /* CIRCLED DIGIT NINE */
	case 0xB5: return 0x2469;    /* CIRCLED NUMBER TEN */
	case 0xB6: return 0x2776;    /* DINGBAT NEGATIVE CIRCLED DIGIT ONE */
	case 0xB7: return 0x2777;    /* DINGBAT NEGATIVE CIRCLED DIGIT TWO */
	case 0xB8: return 0x2778;    /* DINGBAT NEGATIVE CIRCLED DIGIT THREE */
	case 0xB9: return 0x2779;    /* DINGBAT NEGATIVE CIRCLED DIGIT FOUR */
	case 0xBA: return 0x277A;    /* DINGBAT NEGATIVE CIRCLED DIGIT FIVE */
	case 0xBB: return 0x277B;    /* DINGBAT NEGATIVE CIRCLED DIGIT SIX */
	case 0xBC: return 0x277C;    /* DINGBAT NEGATIVE CIRCLED DIGIT SEVEN */
	case 0xBD: return 0x277D;    /* DINGBAT NEGATIVE CIRCLED DIGIT EIGHT */
	case 0xBE: return 0x277E;    /* DINGBAT NEGATIVE CIRCLED DIGIT NINE */
	case 0xBF: return 0x277F;    /* DINGBAT NEGATIVE CIRCLED NUMBER TEN */
	case 0xC0: return 0x2780;    /* DINGBAT CIRCLED SANS-SERIF DIGIT ONE */
	case 0xC1: return 0x2781;    /* DINGBAT CIRCLED SANS-SERIF DIGIT TWO */
	case 0xC2: return 0x2782;    /* DINGBAT CIRCLED SANS-SERIF DIGIT THREE */
	case 0xC3: return 0x2783;    /* DINGBAT CIRCLED SANS-SERIF DIGIT FOUR */
	case 0xC4: return 0x2784;    /* DINGBAT CIRCLED SANS-SERIF DIGIT FIVE */
	case 0xC5: return 0x2785;    /* DINGBAT CIRCLED SANS-SERIF DIGIT SIX */
	case 0xC6: return 0x2786;    /* DINGBAT CIRCLED SANS-SERIF DIGIT SEVEN */
	case 0xC7: return 0x2787;    /* DINGBAT CIRCLED SANS-SERIF DIGIT EIGHT */
	case 0xC8: return 0x2788;    /* DINGBAT CIRCLED SANS-SERIF DIGIT NINE */
	case 0xC9: return 0x2789;    /* DINGBAT CIRCLED SANS-SERIF NUMBER TEN */
	case 0xCA: return 0x278A;    /* DINGBAT NEGATIVE CIRCLED SANS-SERIF DIGIT ONE */
	case 0xCB: return 0x278B;    /* DINGBAT NEGATIVE CIRCLED SANS-SERIF DIGIT TWO */
	case 0xCC: return 0x278C;    /* DINGBAT NEGATIVE CIRCLED SANS-SERIF DIGIT THREE */
	case 0xCD: return 0x278D;    /* DINGBAT NEGATIVE CIRCLED SANS-SERIF DIGIT FOUR */
	case 0xCE: return 0x278E;    /* DINGBAT NEGATIVE CIRCLED SANS-SERIF DIGIT FIVE */
	case 0xCF: return 0x278F;    /* DINGBAT NEGATIVE CIRCLED SANS-SERIF DIGIT SIX */
	case 0xD0: return 0x2790;    /* DINGBAT NEGATIVE CIRCLED SANS-SERIF DIGIT SEVEN */
	case 0xD1: return 0x2791;    /* DINGBAT NEGATIVE CIRCLED SANS-SERIF DIGIT EIGHT */
	case 0xD2: return 0x2792;    /* DINGBAT NEGATIVE CIRCLED SANS-SERIF DIGIT NINE */
	case 0xD3: return 0x2793;    /* DINGBAT NEGATIVE CIRCLED SANS-SERIF NUMBER TEN */
	case 0xD4: return 0x2794;    /* HEAVY WIDE-HEADED RIGHTWARDS ARROW */
	case 0xD5: return 0x2192;    /* RIGHTWARDS ARROW */
	case 0xD6: return 0x2194;    /* LEFT RIGHT ARROW */
	case 0xD7: return 0x2195;    /* UP DOWN ARROW */
	case 0xD8: return 0x2798;    /* HEAVY SOUTH EAST ARROW */
	case 0xD9: return 0x2799;    /* HEAVY RIGHTWARDS ARROW */
	case 0xDA: return 0x279A;    /* HEAVY NORTH EAST ARROW */
	case 0xDB: return 0x279B;    /* DRAFTING POINT RIGHTWARDS ARROW */
	case 0xDC: return 0x279C;    /* HEAVY ROUND-TIPPED RIGHTWARDS ARROW */
	case 0xDD: return 0x279D;    /* TRIANGLE-HEADED RIGHTWARDS ARROW */
	case 0xDE: return 0x279E;    /* HEAVY TRIANGLE-HEADED RIGHTWARDS ARROW */
	case 0xDF: return 0x279F;    /* DASHED TRIANGLE-HEADED RIGHTWARDS ARROW */
	case 0xE0: return 0x27A0;    /* HEAVY DASHED TRIANGLE-HEADED RIGHTWARDS ARROW */
	case 0xE1: return 0x27A1;    /* BLACK RIGHTWARDS ARROW */
	case 0xE2: return 0x27A2;    /* THREE-D TOP-LIGHTED RIGHTWARDS ARROWHEAD */
	case 0xE3: return 0x27A3;    /* THREE-D BOTTOM-LIGHTED RIGHTWARDS ARROWHEAD */
	case 0xE4: return 0x27A4;    /* BLACK RIGHTWARDS ARROWHEAD */
	case 0xE5: return 0x27A5;    /* HEAVY BLACK CURVED DOWNWARDS AND RIGHTWARDS ARROW */
	case 0xE6: return 0x27A6;    /* HEAVY BLACK CURVED UPWARDS AND RIGHTWARDS ARROW */
	case 0xE7: return 0x27A7;    /* SQUAT BLACK RIGHTWARDS ARROW */
	case 0xE8: return 0x27A8;    /* HEAVY CONCAVE-POINTED BLACK RIGHTWARDS ARROW */
	case 0xE9: return 0x27A9;    /* RIGHT-SHADED WHITE RIGHTWARDS ARROW */
	case 0xEA: return 0x27AA;    /* LEFT-SHADED WHITE RIGHTWARDS ARROW */
	case 0xEB: return 0x27AB;    /* BACK-TILTED SHADOWED WHITE RIGHTWARDS ARROW */
	case 0xEC: return 0x27AC;    /* FRONT-TILTED SHADOWED WHITE RIGHTWARDS ARROW */
	case 0xED: return 0x27AD;    /* HEAVY LOWER RIGHT-SHADOWED WHITE RIGHTWARDS ARROW */
	case 0xEE: return 0x27AE;    /* HEAVY UPPER RIGHT-SHADOWED WHITE RIGHTWARDS ARROW */
	case 0xEF: return 0x27AF;    /* NOTCHED LOWER RIGHT-SHADOWED WHITE RIGHTWARDS ARROW */
	case 0xF1: return 0x27B1;    /* NOTCHED UPPER RIGHT-SHADOWED WHITE RIGHTWARDS ARROW */
	case 0xF2: return 0x27B2;    /* CIRCLED HEAVY WHITE RIGHTWARDS ARROW */
	case 0xF3: return 0x27B3;    /* WHITE-FEATHERED RIGHTWARDS ARROW */
	case 0xF4: return 0x27B4;    /* BLACK-FEATHERED SOUTH EAST ARROW */
	case 0xF5: return 0x27B5;    /* BLACK-FEATHERED RIGHTWARDS ARROW */
	case 0xF6: return 0x27B6;    /* BLACK-FEATHERED NORTH EAST ARROW */
	case 0xF7: return 0x27B7;    /* HEAVY BLACK-FEATHERED SOUTH EAST ARROW */
	case 0xF8: return 0x27B8;    /* HEAVY BLACK-FEATHERED RIGHTWARDS ARROW */
	case 0xF9: return 0x27B9;    /* HEAVY BLACK-FEATHERED NORTH EAST ARROW */
	case 0xFA: return 0x27BA;    /* TEARDROP-BARBED RIGHTWARDS ARROW */
	case 0xFB: return 0x27BB;    /* HEAVY TEARDROP-SHANKED RIGHTWARDS ARROW */
	case 0xFC: return 0x27BC;    /* WEDGE-TAILED RIGHTWARDS ARROW */
	case 0xFD: return 0x27BD;    /* HEAVY WEDGE-TAILED RIGHTWARDS ARROW */
	case 0xFE: return 0x27BE;    /* OPEN-OUTLINED RIGHTWARDS ARROW */
	default:   return 0x0000;
	}
}

/*
 * Unicode mapping found in
 * /usr/share/fonts/encodings/adobe-symbol.enc.gz
 * Duplicates are disabled
 */
XftChar32
map_symbols(XftChar8 in)
{
	switch(in) {
	case 0x20: return 0x0020;    /* SPACE */
	case 0x21: return 0x0021;    /* EXCLAMATION MARK */
	case 0x22: return 0x2200;    /* FOR ALL */
	case 0x23: return 0x0023;    /* NUMBER SIGN */
	case 0x24: return 0x2203;    /* THERE EXISTS */
	case 0x25: return 0x0025;    /* PERCENT SIGN */
	case 0x26: return 0x0026;    /* AMPERSAND */
	case 0x27: return 0x220B;    /* CONTAINS AS MEMBER */
	case 0x28: return 0x0028;    /* OPENING PARENTHESIS */
	case 0x29: return 0x0029;    /* CLOSING PARENTHESIS */
	case 0x2A: return 0x2217;    /* ASTERISK OPERATOR */
	case 0x2B: return 0x002B;    /* PLUS SIGN */
	case 0x2C: return 0x002C;    /* COMMA */
	case 0x2D: return 0x2212;    /* MINUS SIGN */
	case 0x2E: return 0x002E;    /* PERIOD */
	case 0x2F: return 0x002F;    /* SLASH */
	case 0x30: return 0x0030;    /* DIGIT ZERO */
	case 0x31: return 0x0031;    /* DIGIT ONE */
	case 0x32: return 0x0032;    /* DIGIT TWO */
	case 0x33: return 0x0033;    /* DIGIT THREE */
	case 0x34: return 0x0034;    /* DIGIT FOUR */
	case 0x35: return 0x0035;    /* DIGIT FIVE */
	case 0x36: return 0x0036;    /* DIGIT SIX */
	case 0x37: return 0x0037;    /* DIGIT SEVEN */
	case 0x38: return 0x0038;    /* DIGIT EIGHT */
	case 0x39: return 0x0039;    /* DIGIT NINE */
	case 0x3A: return 0x003A;    /* COLON */
	case 0x3B: return 0x003B;    /* SEMICOLON */
	case 0x3C: return 0x003C;    /* LESS-THAN SIGN */
	case 0x3D: return 0x003D;    /* EQUALS SIGN */
	case 0x3E: return 0x003E;    /* GREATER-THAN SIGN */
	case 0x3F: return 0x003F;    /* QUESTION MARK */
	case 0x40: return 0x2245;    /* APPROXIMATELY EQUAL TO */
	case 0x41: return 0x0391;    /* GREEK CAPITAL LETTER ALPHA */
	case 0x42: return 0x0392;    /* GREEK CAPITAL LETTER BETA */
	case 0x43: return 0x03A7;    /* GREEK CAPITAL LETTER CHI */
	case 0x44: return 0x0394;    /* GREEK CAPITAL LETTER DELTA */
/*	case 0x44: return 0x2206;     * INCREMENT */
	case 0x45: return 0x0395;    /* GREEK CAPITAL LETTER EPSILON */
	case 0x46: return 0x03A6;    /* GREEK CAPITAL LETTER PHI */
	case 0x47: return 0x0393;    /* GREEK CAPITAL LETTER GAMMA */
	case 0x48: return 0x0397;    /* GREEK CAPITAL LETTER ETA */
	case 0x49: return 0x0399;    /* GREEK CAPITAL LETTER IOTA */
	case 0x4A: return 0x03D1;    /* GREEK SMALL LETTER SCRIPT THETA */
	case 0x4B: return 0x039A;    /* GREEK CAPITAL LETTER KAPPA */
	case 0x4C: return 0x039B;    /* GREEK CAPITAL LETTER LAMBDA */
	case 0x4D: return 0x039C;    /* GREEK CAPITAL LETTER MU */
	case 0x4E: return 0x039D;    /* GREEK CAPITAL LETTER NU */
	case 0x4F: return 0x039F;    /* GREEK CAPITAL LETTER OMICRON */
	case 0x50: return 0x03A0;    /* GREEK CAPITAL LETTER PI */
	case 0x51: return 0x0398;    /* GREEK CAPITAL LETTER THETA */
	case 0x52: return 0x03A1;    /* GREEK CAPITAL LETTER RHO */
	case 0x53: return 0x03A3;    /* GREEK CAPITAL LETTER SIGMA */
	case 0x54: return 0x03A4;    /* GREEK CAPITAL LETTER TAU */
	case 0x55: return 0x03A5;    /* GREEK CAPITAL LETTER UPSILON */
	case 0x56: return 0x03C2;    /* GREEK SMALL LETTER FINAL SIGMA */
	case 0x57: return 0x03A9;    /* GREEK CAPITAL LETTER OMEGA */
/*	case 0x57: return 0x2126;     * OHM */
	case 0x58: return 0x039E;    /* GREEK CAPITAL LETTER XI */
	case 0x59: return 0x03A8;    /* GREEK CAPITAL LETTER PSI */
	case 0x5A: return 0x0396;    /* GREEK CAPITAL LETTER ZETA */
	case 0x5B: return 0x005B;    /* OPENING SQUARE BRACKET */
	case 0x5C: return 0x2234;    /* THEREFORE */
	case 0x5D: return 0x005D;    /* CLOSING SQUARE BRACKET */
	case 0x5E: return 0x22A5;    /* UP TACK */
	case 0x5F: return 0x005F;    /* SPACING UNDERSCORE */
	case 0x60: return 0x203E;    /* SPACING OVERSCORE */
	case 0x61: return 0x03B1;    /* GREEK SMALL LETTER ALPHA */
	case 0x62: return 0x03B2;    /* GREEK SMALL LETTER BETA */
	case 0x63: return 0x03C7;    /* GREEK SMALL LETTER CHI */
	case 0x64: return 0x03B4;    /* GREEK SMALL LETTER DELTA */
	case 0x65: return 0x03B5;    /* GREEK SMALL LETTER EPSILON */
	case 0x66: return 0x03C6;    /* GREEK SMALL LETTER PHI */
	case 0x67: return 0x03B3;    /* GREEK SMALL LETTER GAMMA */
	case 0x68: return 0x03B7;    /* GREEK SMALL LETTER ETA */
	case 0x69: return 0x03B9;    /* GREEK SMALL LETTER IOTA */
	case 0x6A: return 0x03D5;    /* GREEK SMALL LETTER SCRIPT PHI */
	case 0x6B: return 0x03BA;    /* GREEK SMALL LETTER KAPPA */
	case 0x6C: return 0x03BB;    /* GREEK SMALL LETTER LAMBDA */
	case 0x6D: return 0x03BC;    /* GREEK SMALL LETTER MU */
	case 0x6E: return 0x03BD;    /* GREEK SMALL LETTER NU */
	case 0x6F: return 0x03BF;    /* GREEK SMALL LETTER OMICRON */
	case 0x70: return 0x03C0;    /* GREEK SMALL LETTER PI */
	case 0x71: return 0x03B8;    /* GREEK SMALL LETTER THETA */
	case 0x72: return 0x03C1;    /* GREEK SMALL LETTER RHO */
	case 0x73: return 0x03C3;    /* GREEK SMALL LETTER SIGMA */
	case 0x74: return 0x03C4;    /* GREEK SMALL LETTER TAU */
	case 0x75: return 0x03C5;    /* GREEK SMALL LETTER UPSILON */
	case 0x76: return 0x03D6;    /* GREEK SMALL LETTER OMEGA PI */
	case 0x77: return 0x03C9;    /* GREEK SMALL LETTER OMEGA */
	case 0x78: return 0x03BE;    /* GREEK SMALL LETTER XI */
	case 0x79: return 0x03C8;    /* GREEK SMALL LETTER PSI */
	case 0x7A: return 0x03B6;    /* GREEK SMALL LETTER ZETA */
	case 0x7B: return 0x007B;    /* OPENING CURLY BRACKET */
	case 0x7C: return 0x007C;    /* VERTICAL BAR */
	case 0x7D: return 0x007D;    /* CLOSING CURLY BRACKET */
	case 0x7E: return 0x223C;    /* TILDE OPERATOR */
	/* case 0x7F */
	case 0x80: return 0xf8ff;    /* Apple logo, private use area. */
/*	case 0x80: return 0xf000;     * Another location for apple logo. */
	/* 0x81 - 0xA0 */
	case 0xA0: return 0x20ac;    /* Euro sign */
	case 0xA1: return 0x03D2;    /* GREEK CAPITAL LETTER UPSILON HOOK */
	case 0xA2: return 0x2032;    /* PRIME */
	case 0xA3: return 0x2264;    /* LESS THAN OR EQUAL TO */
	case 0xA4: return 0x2044;    /* FRACTION SLASH */
/*	case 0xA4: return 0x2215;     * DIVISION SLASH */
	case 0xA5: return 0x221E;    /* INFINITY */
	case 0xA6: return 0x0192;    /* LATIN SMALL LETTER SCRIPT F */
	case 0xA7: return 0x2663;    /* BLACK CLUB SUIT */
	case 0xA8: return 0x2666;    /* BLACK DIAMOND SUIT */
	case 0xA9: return 0x2665;    /* BLACK HEART SUIT */
	case 0xAA: return 0x2660;    /* BLACK SPADE SUIT */
	case 0xAB: return 0x2194;    /* LEFT RIGHT ARROW */
	case 0xAC: return 0x2190;    /* LEFT ARROW */
	case 0xAD: return 0x2191;    /* UP ARROW */
	case 0xAE: return 0x2192;    /* RIGHT ARROW */
	case 0xAF: return 0x2193;    /* DOWN ARROW */
	case 0xB0: return 0x00B0;    /* DEGREE SIGN */
	case 0xB1: return 0x00B1;    /* PLUS-OR-MINUS SIGN */
	case 0xB2: return 0x2033;    /* DOUBLE PRIME */
	case 0xB3: return 0x2265;    /* GREATER THAN OR EQUAL TO */
	case 0xB4: return 0x00D7;    /* MULTIPLICATION SIGN */
	case 0xB5: return 0x221D;    /* PROPORTIONAL TO */
	case 0xB6: return 0x2202;    /* PARTIAL DIFFERENTIAL */
	case 0xB7: return 0x2022;    /* BULLET */
	case 0xB8: return 0x00F7;    /* DIVISION SIGN */
	case 0xB9: return 0x2260;    /* NOT EQUAL TO */
	case 0xBA: return 0x2261;    /* IDENTICAL TO */
	case 0xBB: return 0x2248;    /* ALMOST EQUAL TO */
	case 0xBC: return 0x2026;    /* HORIZONTAL ELLIPSIS */
	/* Vertical and horizontal line extension are supposed to be used for
	   extension of arrows; Alternatives would be box drawing signs */
	case 0xBD: return 0x23d0;    /* Vertical line extension */
	case 0xBE: return 0x23af;    /* Horizontal line extension */
/*	case 0xBD: return 0x2502;     * Box drawings light vertical */
/*	case 0xBE: return 0x2500;     * Box drawings light horizontal */
	case 0xBF: return 0x21B5;    /* DOWN ARROW WITH CORNER LEFT */
	case 0xC0: return 0x2135;    /* FIRST TRANSFINITE CARDINAL */
	case 0xC1: return 0x2111;    /* BLACK-LETTER I */
	case 0xC2: return 0x211C;    /* BLACK-LETTER R */
	case 0xC3: return 0x2118;    /* SCRIPT P */
	case 0xC4: return 0x2297;    /* CIRCLED TIMES */
	case 0xC5: return 0x2295;    /* CIRCLED PLUS */
	case 0xC6: return 0x2205;    /* EMPTY SET */
	case 0xC7: return 0x2229;    /* INTERSECTION */
	case 0xC8: return 0x222A;    /* UNION */
	case 0xC9: return 0x2283;    /* SUPERSET OF */
	case 0xCA: return 0x2287;    /* SUPERSET OF OR EQUAL TO */
	case 0xCB: return 0x2284;    /* NOT A SUBSET OF */
	case 0xCC: return 0x2282;    /* SUBSET OF */
	case 0xCD: return 0x2286;    /* SUBSET OF OR EQUAL TO */
	case 0xCE: return 0x2208;    /* ELEMENT OF */
	case 0xCF: return 0x2209;    /* NOT AN ELEMENT OF */
	case 0xD0: return 0x2220;    /* ANGLE */
	case 0xD1: return 0x2207;    /* NABLA */
	case 0xD2: return 0x00AE;    /* REGISTERED TRADE MARK SIGN */
	case 0xD3: return 0x00A9;    /* COPYRIGHT SIGN */
	case 0xD4: return 0x2122;    /* TRADEMARK */
	case 0xD5: return 0x220F;    /* N-ARY PRODUCT */
	case 0xD6: return 0x221A;    /* SQUARE ROOT */
	case 0xD7: return 0x22C5;    /* DOT OPERATOR */
	case 0xD8: return 0x00AC;    /* NOT SIGN */
	case 0xD9: return 0x2227;    /* LOGICAL AND */
	case 0xDA: return 0x2228;    /* LOGICAL OR */
	case 0xDB: return 0x21D4;    /* LEFT RIGHT DOUBLE ARROW */
	case 0xDC: return 0x21D0;    /* LEFT DOUBLE ARROW */
	case 0xDD: return 0x21D1;    /* UP DOUBLE ARROW */
	case 0xDE: return 0x21D2;    /* RIGHT DOUBLE ARROW */
	case 0xDF: return 0x21D3;    /* DOWN DOUBLE ARROW */
	case 0xE0: return 0x25CA;    /* LOZENGE */
	case 0xE1: return 0x2329;    /* BRA */
	case 0xE2: return 0x00AE;    /* REGISTERED TRADE MARK SIGN */
	case 0xE3: return 0x00A9;    /* COPYRIGHT SIGN */
	case 0xE4: return 0x2122;    /* TRADEMARK */
	case 0xE5: return 0x2211;    /* N-ARY SUMMATION */
	case 0xE6: return 0x239b;    /* Left parenthesis upper hook */
	case 0xE7: return 0x239c;    /* Left parenthesis extension */
	case 0xE8: return 0x239d;    /* Left parenthesis lower hook */
	case 0xE9: return 0x23a1;    /* Left square bracket upper corner */
	case 0xEA: return 0x23a2;    /* Left square bracket extension */
	case 0xEB: return 0x23a3;    /* Left square bracket lower corner */
	case 0xEC: return 0x23a7;    /* Left curly bracket upper hook */
	case 0xED: return 0x23a8;    /* Left curly bracket middle piece */
	case 0xEE: return 0x23a9;    /* Left curly bracket lower hook */
	case 0xEF: return 0x23aa;    /* Curly bracket extension */
	/* 0xF0 */
	case 0xF1: return 0x232A;    /* KET */
	case 0xF2: return 0x222B;    /* INTEGRAL */
	case 0xF3: return 0x2320;    /* TOP HALF INTEGRAL */
	case 0xF4: return 0x23ae;    /* Integral extension */
	case 0xF5: return 0x2321;    /* BOTTOM HALF INTEGRAL */
	case 0xF6: return 0x239e;    /* Right parenthesis upper hook */
	case 0xF7: return 0x239f;    /* Right parenthesis extension */
	case 0xF8: return 0x23a0;    /* Right parenthesis lower hook */
	case 0xF9: return 0x23a4;    /* Right square bracket upper corner */
	case 0xFA: return 0x23a5;    /* Right square bracket extension */
	case 0xFB: return 0x23a6;    /* Right square bracket lower corner */
	case 0xFC: return 0x23ab;    /* Right curly bracket upper hook */
	case 0xFD: return 0x23ac;    /* Right curly bracket middle piece */
	case 0xFE: return 0x23ad;    /* Right curly bracket lower hook */
	/*  0xFF */
	default:   return 0x0000;
	}
}

static XftChar32
(* _charmap(int fnum, XftFont *xft_font))(XftChar8)
{
	int		f;
	XftChar32	(*maps[2])(XftChar8) = {map_symbols, map_dingbats};
	static struct {
		char	isinitialized;
		XftChar32 (*map)(XftChar8);
	} font_map[2];

	f = fnum == FONT_SYMBOL ? 0 : 1;
	if (!font_map[f].isinitialized)
		font_map[f].map =
			XftCharExists(tool_d, xft_font, 0x61) ? NULL : maps[f];
	return font_map[f].map;
}

void
font_drawtext(XftDraw *xftdraw, XftFont *xft_font, int fnum /* text->font */,
		int x, int y, const char *s, const Color c)
{
	XftChar32	(*map)(XftChar8);

	if ((fnum != FONT_SYMBOL && fnum != FONT_DINGBATS) ||
			(map = _charmap(fnum, xft_font)) == NULL) {
		XftDrawStringUtf8(xftdraw, &xftcolor[c], xft_font, x, y,
				(XftChar8*)s, (int)strlen(s));
	} else { /* map != NULL */
		XftChar8	*chr;
		int		glen = 0;
		size_t		len = strlen(s);
		XftChar32	glyph;
		XftChar32	glyphs[len];

		for (chr = (XftChar8 *)s; chr < (XftChar8 *)s + len; ++chr) {
			glyph = XftCharIndex(tool_d, xft_font, map(*chr));
			if (glyph)
				glyphs[glen++] = glyph;
		}
		XftDrawGlyphs(xftdraw, &xftcolor[c], xft_font, x, y, glyphs,
				glen);
	}
}
