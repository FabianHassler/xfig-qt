/*
 * xlib_qt.cpp - the subset of Xlib and Xft used by xfig, implemented with Qt.
 *
 * Drawables are QImages.  Drawing follows X semantics: no antialiasing for
 * graphics (so that XOR rubber-banding and erase-by-redraw stay exact),
 * GXxor maps to a raster XOR, stipples become QBitmap brushes.  Xft text is
 * rendered from FreeType outlines selected through fontconfig, so fonts are
 * chosen exactly as in the X11 version.
 */
#include <QBitmap>
#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QApplication>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "xq.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

/* ------------------------------------------------------------ drawables */

struct DrawableRec {
	QImage	img;
	int	depth;
	bool	window;
	Pixmap	background = None;	/* windows: background pixmap */
	QRgb	bgcolor = 0xffffffffu;	/* windows: background pixel */
};

static std::unordered_map<XID, DrawableRec *> drawables;
static XID	next_xid = 0x100;
static std::function<void(Window)> damage_cb;
static std::function<void(Window, Cursor)> cursor_cb;

struct _XDisplay { int dummy; };
static _XDisplay the_display;
extern "C" Display *xq_display = &the_display;

static DrawableRec *
lookup(Drawable d)
{
	auto it = drawables.find(d);
	return it == drawables.end() ? nullptr : it->second;
}

int
xq_drawable_depth(Drawable d)
{
	DrawableRec *r = lookup(d);
	return r ? r->depth : 0;
}

QImage *
xq_image(Drawable d)
{
	DrawableRec *r = lookup(d);
	return r ? &r->img : nullptr;
}

void
xq_set_damage_callback(std::function<void(Window)> cb)
{
	damage_cb = std::move(cb);
}

void
xq_set_cursor_callback(std::function<void(Window, Cursor)> cb)
{
	cursor_cb = std::move(cb);
}

static void
damage(Drawable d, DrawableRec *r)
{
	if (r->window && damage_cb)
		damage_cb(d);
}

static QImage
new_image(int w, int h, double dpr, int depth)
{
	if (w < 1) w = 1;
	if (h < 1) h = 1;
	QImage img(int(std::ceil(w * dpr)), int(std::ceil(h * dpr)),
			depth == 1 ? QImage::Format_RGB32 : QImage::Format_RGB32);
	img.setDevicePixelRatio(dpr);
	img.fill(depth == 1 ? Qt::black : Qt::white);
	return img;
}

extern "C" Window
xq_create_window(int width, int height, double dpr)
{
	DrawableRec *r = new DrawableRec{new_image(width, height, dpr, 24), 24, true};
	XID id = next_xid++;
	drawables[id] = r;
	return id;
}

extern "C" void
xq_resize_window(Window w, int width, int height, double dpr)
{
	DrawableRec *r = lookup(w);
	if (!r)
		return;
	QImage img = new_image(width, height, dpr, 24);
	QPainter p(&img);
	p.drawImage(0, 0, r->img);
	p.end();
	r->img = img;
}

static XID
add_drawable(QImage img, int depth)
{
	XID id = next_xid++;
	drawables[id] = new DrawableRec{std::move(img), depth, false};
	return id;
}

extern "C" Pixmap
XCreatePixmap(Display *, Drawable, unsigned w, unsigned h, unsigned depth)
{
	return add_drawable(new_image(w, h, 1.0, depth), depth);
}

/* X bitmap data: rows padded to bytes, least significant bit first */
static QImage
bitmap_image(const char *data, unsigned w, unsigned h, QRgb on, QRgb off)
{
	QImage img(w ? w : 1, h ? h : 1, QImage::Format_RGB32);
	int bpl = (w + 7) / 8;
	for (unsigned y = 0; y < h; ++y) {
		QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
		const unsigned char *row = (const unsigned char *)data + y * bpl;
		for (unsigned x = 0; x < w; ++x)
			line[x] = (row[x >> 3] >> (x & 7)) & 1 ? on : off;
	}
	return img;
}

/* A depth-1 pixmap as a QBitmap: set bits (any non-black pixel) become
   color1.  QBitmap::fromImage() maps by color, and the mask from
   createMaskFromColor() has white at index 1, so mask the unset (black)
   pixels in: they become white, i.e. color0. */
static QBitmap
bitmap_of(const QImage &img)
{
	return QBitmap::fromImage(img.createMaskFromColor(0xff000000u,
				Qt::MaskInColor));
}

static inline QRgb
pix2rgb(unsigned long p)
{
	return 0xff000000u | (QRgb)(p & 0xffffff);
}

extern "C" Pixmap
XCreateBitmapFromData(Display *, Drawable, const char *data, unsigned w, unsigned h)
{
	return add_drawable(bitmap_image(data, w, h, 0xffffffffu, 0xff000000u), 1);
}

extern "C" Pixmap
XCreatePixmapFromBitmapData(Display *, Drawable, char *data, unsigned w,
		unsigned h, unsigned long fg, unsigned long bg, unsigned depth)
{
	return add_drawable(bitmap_image(data, w, h, pix2rgb(fg), pix2rgb(bg)),
			depth);
}

extern "C" int
XReadBitmapFromFile(Display *, Drawable, const char *file, unsigned *w,
		unsigned *h, Pixmap *pm, int *xhot, int *yhot)
{
	QImage img(QString::fromLocal8Bit(file), "XBM");
	if (img.isNull())
		return BitmapOpenFailed;
	img = img.convertToFormat(QImage::Format_RGB32);
	/* in a loaded XBM, set bits are black (color 1) */
	for (int y = 0; y < img.height(); ++y) {
		QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
		for (int x = 0; x < img.width(); ++x)
			line[x] = qGray(line[x]) < 128 ? 0xffffffffu : 0xff000000u;
	}
	*w = img.width();
	*h = img.height();
	if (xhot) *xhot = -1;
	if (yhot) *yhot = -1;
	*pm = add_drawable(img, 1);
	return BitmapSuccess;
}

extern "C" int
XFreePixmap(Display *, Pixmap p)
{
	auto it = drawables.find(p);
	if (it != drawables.end() && !it->second->window) {
		delete it->second;
		drawables.erase(it);
	}
	return 0;
}

extern "C" int
XGetGeometry(Display *, Drawable d, Window *root, int *x, int *y, unsigned *w,
		unsigned *h, unsigned *bw, unsigned *depth)
{
	DrawableRec *r = lookup(d);
	if (!r)
		return 0;
	if (root) *root = 0;
	if (x) *x = 0;
	if (y) *y = 0;
	if (w) *w = r->img.deviceIndependentSize().width();
	if (h) *h = r->img.deviceIndependentSize().height();
	if (bw) *bw = 0;
	if (depth) *depth = r->depth;
	return 1;
}

/* ------------------------------------------------------------------- GC */

struct _XGC {
	XGCValues	v;
	std::vector<qreal> dashes;
	bool		has_clip = false;
	QRegion		clip;
};

static void
change_gc(GC gc, unsigned long mask, const XGCValues *v)
{
	XGCValues &g = gc->v;
	if (mask & GCFunction)	g.function = v->function;
	if (mask & GCPlaneMask)	g.plane_mask = v->plane_mask;
	if (mask & GCForeground) g.foreground = v->foreground;
	if (mask & GCBackground) g.background = v->background;
	if (mask & GCLineWidth)	g.line_width = v->line_width;
	if (mask & GCLineStyle)	g.line_style = v->line_style;
	if (mask & GCCapStyle)	g.cap_style = v->cap_style;
	if (mask & GCJoinStyle)	g.join_style = v->join_style;
	if (mask & GCFillStyle)	g.fill_style = v->fill_style;
	if (mask & GCFillRule)	g.fill_rule = v->fill_rule;
	if (mask & GCTile)	g.tile = v->tile;
	if (mask & GCStipple)	g.stipple = v->stipple;
	if (mask & GCTileStipXOrigin) g.ts_x_origin = v->ts_x_origin;
	if (mask & GCTileStipYOrigin) g.ts_y_origin = v->ts_y_origin;
	if (mask & GCFont)	g.font = v->font;
	if (mask & GCSubwindowMode) g.subwindow_mode = v->subwindow_mode;
	if (mask & GCGraphicsExposures) g.graphics_exposures = v->graphics_exposures;
	if (mask & GCClipXOrigin) g.clip_x_origin = v->clip_x_origin;
	if (mask & GCClipYOrigin) g.clip_y_origin = v->clip_y_origin;
	if (mask & GCDashOffset) g.dash_offset = v->dash_offset;
	if (mask & GCArcMode)	g.arc_mode = v->arc_mode;
	if (mask & GCDashList) {
		g.dashes = v->dashes;
		gc->dashes = {qreal((unsigned char)v->dashes),
			qreal((unsigned char)v->dashes)};
	}
	if (mask & GCClipMask) {
		g.clip_mask = v->clip_mask;
		gc->has_clip = false;
		if (v->clip_mask != None) {
			DrawableRec *r = lookup(v->clip_mask);
			if (r) {
				gc->clip = QRegion(bitmap_of(r->img));
				gc->has_clip = true;
			}
		}
	}
}

extern "C" GC
XCreateGC(Display *, Drawable, unsigned long mask, XGCValues *v)
{
	GC gc = new _XGC;
	XGCValues &g = gc->v;
	memset(&g, 0, sizeof g);
	g.function = GXcopy;
	g.plane_mask = ~0UL;
	g.foreground = 0;
	g.background = 0xffffff;
	g.line_width = 0;
	g.line_style = LineSolid;
	g.cap_style = CapButt;
	g.join_style = JoinMiter;
	g.fill_style = FillSolid;
	g.fill_rule = EvenOddRule;
	g.arc_mode = ArcPieSlice;
	g.graphics_exposures = True;
	g.dashes = 4;
	gc->dashes = {4, 4};
	if (v)
		change_gc(gc, mask, v);
	return gc;
}

extern "C" int
XChangeGC(Display *, GC gc, unsigned long mask, XGCValues *v)
{
	if (!gc)
		return 0;
	change_gc(gc, mask, v);
	return 1;
}

extern "C" int
XCopyGC(Display *, GC src, unsigned long mask, GC dst)
{
	change_gc(dst, mask & ~(GCClipMask | GCDashList), &src->v);
	if (mask & GCClipMask) {
		dst->has_clip = src->has_clip;
		dst->clip = src->clip;
		dst->v.clip_mask = src->v.clip_mask;
	}
	if (mask & GCDashList)
		dst->dashes = src->dashes;
	return 1;
}

extern "C" int
XGetGCValues(Display *, GC gc, unsigned long, XGCValues *v)
{
	*v = gc->v;
	return 1;
}

extern "C" int XFreeGC(Display *, GC gc) { delete gc; return 1; }
extern "C" int XSetForeground(Display *, GC gc, unsigned long p) { if (!gc) return 0; gc->v.foreground = p; return 1; }
extern "C" int XSetBackground(Display *, GC gc, unsigned long p) { if (!gc) return 0; gc->v.background = p; return 1; }
extern "C" int XSetFunction(Display *, GC gc, int f) { if (!gc) return 0; gc->v.function = f; return 1; }
extern "C" int XSetPlaneMask(Display *, GC gc, unsigned long m) { if (!gc) return 0; gc->v.plane_mask = m; return 1; }
extern "C" int XSetFillStyle(Display *, GC gc, int s) { if (!gc) return 0; gc->v.fill_style = s; return 1; }
extern "C" int XSetStipple(Display *, GC gc, Pixmap p) { if (!gc) return 0; gc->v.stipple = p; return 1; }
extern "C" int XSetTile(Display *, GC gc, Pixmap p) { if (!gc) return 0; gc->v.tile = p; return 1; }
extern "C" int XSetFont(Display *, GC gc, Font f) { if (!gc) return 0; gc->v.font = f; return 1; }
extern "C" int
XSetTSOrigin(Display *, GC gc, int x, int y)
{
	if (!gc)
		return 0;
	gc->v.ts_x_origin = x;
	gc->v.ts_y_origin = y;
	return 1;
}

extern "C" int
XSetLineAttributes(Display *, GC gc, unsigned w, int ls, int cs, int js)
{
	if (!gc)
		return 0;
	gc->v.line_width = w;
	gc->v.line_style = ls;
	gc->v.cap_style = cs;
	gc->v.join_style = js;
	return 1;
}

extern "C" int
XSetDashes(Display *, GC gc, int offset, const char *list, int n)
{
	if (!gc)
		return 0;
	gc->v.dash_offset = offset;
	gc->dashes.clear();
	for (int i = 0; i < n; ++i)
		gc->dashes.push_back(qreal((unsigned char)list[i]));
	if (n % 2)	/* an odd list is repeated */
		for (int i = 0; i < n; ++i)
			gc->dashes.push_back(qreal((unsigned char)list[i]));
	return 1;
}

extern "C" int
XSetClipMask(Display *, GC gc, Pixmap p)
{
	XGCValues v;
	v.clip_mask = p;
	change_gc(gc, GCClipMask, &v);
	return 1;
}

extern "C" int
XSetClipOrigin(Display *, GC gc, int x, int y)
{
	if (!gc)
		return 0;
	gc->v.clip_x_origin = x;
	gc->v.clip_y_origin = y;
	return 1;
}

extern "C" int
XSetClipRectangles(Display *, GC gc, int xo, int yo, XRectangle *r, int n, int)
{
	if (!gc)
		return 0;
	QRegion reg;
	for (int i = 0; i < n; ++i)
		reg += QRect(r[i].x, r[i].y, r[i].width, r[i].height);
	gc->clip = reg;
	gc->has_clip = true;
	gc->v.clip_x_origin = xo;
	gc->v.clip_y_origin = yo;
	gc->v.clip_mask = None;
	return 1;
}

struct _XRegion { QRegion r; };

extern "C" int
XSetRegion(Display *, GC gc, Region r)
{
	if (!gc)
		return 0;
	gc->clip = r->r;
	gc->has_clip = true;
	gc->v.clip_x_origin = gc->v.clip_y_origin = 0;
	gc->v.clip_mask = None;
	return 1;
}

static GC default_gc;

extern "C" GC
XDefaultGC(Display *d, int)
{
	if (!default_gc)
		default_gc = XCreateGC(d, 0, 0, nullptr);
	return default_gc;
}

extern "C" Window XDefaultRootWindow(Display *) { return 0; }

/* -------------------------------------------------------------- painting */

static QPainter::CompositionMode
composition(int function)
{
	switch (function) {
	case GXxor:	return QPainter::RasterOp_SourceXorDestination;
	case GXand:	return QPainter::RasterOp_SourceAndDestination;
	case GXor:	return QPainter::RasterOp_SourceOrDestination;
	case GXinvert:	return QPainter::RasterOp_NotDestination;
	case GXcopyInverted: return QPainter::RasterOp_NotSource;
	default:	return QPainter::CompositionMode_SourceOver;
	}
}

static QBrush
fill_brush(GC gc)
{
	const XGCValues &g = gc->v;
	QColor fg = QColor::fromRgb(pix2rgb(g.foreground));
	if ((g.fill_style == FillStippled || g.fill_style == FillOpaqueStippled)
			&& g.stipple != None) {
		DrawableRec *r = lookup(g.stipple);
		if (r) {
			return QBrush(fg, bitmap_of(r->img));
		}
	}
	if (g.fill_style == FillTiled && g.tile != None) {
		DrawableRec *r = lookup(g.tile);
		if (r)
			return QBrush(r->img);
	}
	return QBrush(fg);
}

/* Paint on drawable d with gc; the callback does the drawing. */
template <class F>
static int
paint(Drawable d, GC gc, F draw, bool antialias = false)
{
	DrawableRec *r = lookup(d);
	if (!r || !gc)
		return 0;
	if (gc->v.function == GXnoop)
		return 1;
	QPainter p(&r->img);
	p.setRenderHint(QPainter::Antialiasing, antialias);
	p.setCompositionMode(composition(gc->v.function));
	if (gc->has_clip)
		p.setClipRegion(gc->clip.translated(gc->v.clip_x_origin,
					gc->v.clip_y_origin));
	draw(p);
	p.end();
	damage(d, r);
	return 1;
}

static QPen
line_pen(GC gc)
{
	const XGCValues &g = gc->v;
	QPen pen(QColor::fromRgb(pix2rgb(g.foreground)));
	qreal w = g.line_width > 0 ? g.line_width : 1;
	pen.setWidthF(w);
	switch (g.cap_style) {
	case CapRound:		pen.setCapStyle(Qt::RoundCap); break;
	case CapProjecting:	pen.setCapStyle(Qt::SquareCap); break;
	default:		pen.setCapStyle(Qt::FlatCap); break;
	}
	switch (g.join_style) {
	case JoinRound:	pen.setJoinStyle(Qt::RoundJoin); break;
	case JoinBevel:	pen.setJoinStyle(Qt::BevelJoin); break;
	default:	pen.setJoinStyle(Qt::MiterJoin); pen.setMiterLimit(10.43); break;
	}
	if (g.line_style != LineSolid && !gc->dashes.empty()) {
		QList<qreal> pat;
		for (qreal v : gc->dashes)
			pat << (v > 0 ? v / w : 1.0 / w);
		pen.setDashPattern(pat);
		pen.setDashOffset(g.dash_offset / w);
	}
	/* a thin line with flat caps would lose its end pixel */
	if (g.line_width <= 1 && pen.capStyle() == Qt::FlatCap)
		pen.setCapStyle(Qt::SquareCap);
	return pen;
}

static void
fill_bg_if_opaque(QPainter &p, GC gc, const QPainterPath &path)
{
	if (gc->v.fill_style == FillOpaqueStippled && gc->v.stipple != None)
		p.fillPath(path, QColor::fromRgb(pix2rgb(gc->v.background)));
}

static void
fill_path(QPainter &p, GC gc, QPainterPath path)
{
	fill_bg_if_opaque(p, gc, path);
	p.setBrushOrigin(gc->v.ts_x_origin, gc->v.ts_y_origin);
	p.fillPath(path, fill_brush(gc));
}

extern "C" int
XDrawPoint(Display *, Drawable d, GC gc, int x, int y)
{
	return paint(d, gc, [&](QPainter &p) {
		p.fillRect(QRectF(x, y, 1, 1), QColor::fromRgb(pix2rgb(gc->v.foreground)));
	});
}

extern "C" int
XDrawPoints(Display *dpy, Drawable d, GC gc, XPoint *pts, int n, int mode)
{
	int x = 0, y = 0;
	return paint(d, gc, [&](QPainter &p) {
		QColor c = QColor::fromRgb(pix2rgb(gc->v.foreground));
		for (int i = 0; i < n; ++i) {
			if (mode == CoordModePrevious && i > 0) {
				x += pts[i].x; y += pts[i].y;
			} else {
				x = pts[i].x; y = pts[i].y;
			}
			p.fillRect(QRectF(x, y, 1, 1), c);
		}
	});
}

/* X centres a line of width w on the pixel centres; offset by half a pixel */
static inline QPointF
px(int x, int y)
{
	return QPointF(x + 0.5, y + 0.5);
}

extern "C" int
XDrawLine(Display *, Drawable d, GC gc, int x1, int y1, int x2, int y2)
{
	return paint(d, gc, [&](QPainter &p) {
		p.setPen(line_pen(gc));
		p.drawLine(px(x1, y1), px(x2, y2));
	});
}

static QPolygonF
polygon(XPoint *pts, int n, int mode, bool centre)
{
	QPolygonF poly;
	poly.reserve(n);
	int x = 0, y = 0;
	for (int i = 0; i < n; ++i) {
		if (mode == CoordModePrevious && i > 0) {
			x += pts[i].x; y += pts[i].y;
		} else {
			x = pts[i].x; y = pts[i].y;
		}
		poly << (centre ? px(x, y) : QPointF(x, y));
	}
	return poly;
}

extern "C" int
XDrawLines(Display *, Drawable d, GC gc, XPoint *pts, int n, int mode)
{
	if (n < 1)
		return 0;
	return paint(d, gc, [&](QPainter &p) {
		p.setPen(line_pen(gc));
		QPolygonF poly = polygon(pts, n, mode, true);
		if (n == 1)
			p.drawPoint(poly[0]);
		else
			p.drawPolyline(poly);
	});
}

extern "C" int
XDrawSegments(Display *, Drawable d, GC gc, XSegment *s, int n)
{
	return paint(d, gc, [&](QPainter &p) {
		p.setPen(line_pen(gc));
		for (int i = 0; i < n; ++i)
			p.drawLine(px(s[i].x1, s[i].y1), px(s[i].x2, s[i].y2));
	});
}

extern "C" int
XDrawRectangle(Display *, Drawable d, GC gc, int x, int y, unsigned w, unsigned h)
{
	return paint(d, gc, [&](QPainter &p) {
		p.setPen(line_pen(gc));
		p.setBrush(Qt::NoBrush);
		p.drawRect(QRectF(x + 0.5, y + 0.5, w, h));
	});
}

extern "C" int
XFillRectangle(Display *, Drawable d, GC gc, int x, int y, unsigned w, unsigned h)
{
	return paint(d, gc, [&](QPainter &p) {
		QPainterPath path;
		path.addRect(x, y, w, h);
		fill_path(p, gc, path);
	});
}

extern "C" int
XFillRectangles(Display *dpy, Drawable d, GC gc, XRectangle *r, int n)
{
	for (int i = 0; i < n; ++i)
		XFillRectangle(dpy, d, gc, r[i].x, r[i].y, r[i].width, r[i].height);
	return 1;
}

extern "C" int
XDrawArc(Display *, Drawable d, GC gc, int x, int y, unsigned w, unsigned h,
		int a1, int a2)
{
	return paint(d, gc, [&](QPainter &p) {
		p.setPen(line_pen(gc));
		p.setBrush(Qt::NoBrush);
		QRectF r(x + 0.5, y + 0.5, w, h);
		if (a2 >= 360 * 64 || a2 <= -360 * 64)
			p.drawEllipse(r);
		else
			p.drawArc(r, a1 / 4, a2 / 4);
	});
}

extern "C" int
XFillArc(Display *, Drawable d, GC gc, int x, int y, unsigned w, unsigned h,
		int a1, int a2)
{
	return paint(d, gc, [&](QPainter &p) {
		QPainterPath path;
		QRectF r(x, y, w, h);
		if (a2 >= 360 * 64 || a2 <= -360 * 64) {
			path.addEllipse(r);
		} else if (gc->v.arc_mode == ArcChord) {
			path.arcMoveTo(r, a1 / 64.0);
			path.arcTo(r, a1 / 64.0, a2 / 64.0);
			path.closeSubpath();
		} else {
			path.moveTo(r.center());
			path.arcTo(r, a1 / 64.0, a2 / 64.0);
			path.closeSubpath();
		}
		fill_path(p, gc, path);
	});
}

extern "C" int
XFillPolygon(Display *, Drawable d, GC gc, XPoint *pts, int n, int, int mode)
{
	if (n < 3)
		return 0;
	return paint(d, gc, [&](QPainter &p) {
		QPainterPath path;
		path.addPolygon(polygon(pts, n, mode, false));
		path.closeSubpath();
		path.setFillRule(gc->v.fill_rule == WindingRule ?
				Qt::WindingFill : Qt::OddEvenFill);
		fill_path(p, gc, path);
	});
}

extern "C" int
XClearArea(Display *, Window w, int x, int y, unsigned wd, unsigned ht, Bool)
{
	DrawableRec *r = lookup(w);
	if (!r)
		return 0;
	QPainter p(&r->img);
	QSizeF sz = r->img.deviceIndependentSize();
	if (wd == 0) wd = sz.width() - x;
	if (ht == 0) ht = sz.height() - y;
	QRect area(x, y, wd, ht);
	DrawableRec *bg = r->background != None ? lookup(r->background) : nullptr;
	if (bg) {
		p.setClipRect(area);
		p.drawTiledPixmap(QRect(QPoint(0, 0), sz.toSize()),
				QPixmap::fromImage(bg->img));
	} else {
		p.fillRect(area, QColor::fromRgb(r->bgcolor));
	}
	p.end();
	damage(w, r);
	return 1;
}

extern "C" int
XSetWindowBackgroundPixmap(Display *, Window w, Pixmap p)
{
	DrawableRec *r = lookup(w);
	if (r)
		r->background = p;
	return 1;
}

extern "C" int
XSetWindowBackground(Display *, Window w, unsigned long pixel)
{
	DrawableRec *r = lookup(w);
	if (r) {
		r->bgcolor = pix2rgb(pixel);
		r->background = None;
	}
	return 1;
}

extern "C" int
XClearWindow(Display *d, Window w)
{
	return XClearArea(d, w, 0, 0, 0, 0, False);
}

extern "C" int
XCopyArea(Display *, Drawable src, Drawable dst, GC gc, int sx, int sy,
		unsigned w, unsigned h, int dx, int dy)
{
	DrawableRec *s = lookup(src);
	if (!s)
		return 0;
	QImage copy;
	if (src == dst) {
		qreal dpr = s->img.devicePixelRatio();
		copy = s->img.copy(QRect(sx * dpr, sy * dpr, w * dpr, h * dpr));
		copy.setDevicePixelRatio(dpr);
		sx = sy = 0;
	}
	const QImage &from = src == dst ? copy : s->img;
	return paint(dst, gc, [&](QPainter &p) {
		p.drawImage(QRectF(dx, dy, w, h), from,
			QRectF(sx * from.devicePixelRatio(), sy * from.devicePixelRatio(),
				w * from.devicePixelRatio(), h * from.devicePixelRatio()));
	});
}

extern "C" int
XCopyPlane(Display *dpy, Drawable src, Drawable dst, GC gc, int sx, int sy,
		unsigned w, unsigned h, int dx, int dy, unsigned long)
{
	DrawableRec *s = lookup(src);
	if (!s)
		return 0;
	/* a bitmap: set bits in foreground, others in background */
	QImage from = s->img.copy(sx, sy, w, h);
	QRgb fg = pix2rgb(gc->v.foreground), bg = pix2rgb(gc->v.background);
	for (int y = 0; y < from.height(); ++y) {
		QRgb *line = reinterpret_cast<QRgb *>(from.scanLine(y));
		for (int x = 0; x < from.width(); ++x)
			line[x] = (line[x] & 0xffffff) ? fg : bg;
	}
	(void)dpy;
	return paint(dst, gc, [&](QPainter &p) { p.drawImage(dx, dy, from); });
}

/* ---------------------------------------------------------------- images */

extern "C" XImage *
XCreateImage(Display *, Visual *, unsigned depth, int format, int offset,
		char *data, unsigned w, unsigned h, int pad, int bpl)
{
	XImage *img = (XImage *)calloc(1, sizeof(XImage));
	img->width = w;
	img->height = h;
	img->xoffset = offset;
	img->format = format;
	img->data = data;
	img->byte_order = LSBFirst;
	img->bitmap_unit = 8;
	img->bitmap_bit_order = LSBFirst;
	img->bitmap_pad = pad;
	img->depth = depth;
	if (format == ZPixmap && depth > 1) {
		img->bits_per_pixel = 32;
		img->red_mask = 0xff0000;
		img->green_mask = 0x00ff00;
		img->blue_mask = 0x0000ff;
	} else {
		img->bits_per_pixel = 1;
	}
	if (bpl == 0) {
		int bits = w * img->bits_per_pixel;
		int padbits = pad > 0 ? pad : 8;
		bpl = ((bits + padbits - 1) / padbits) * padbits / 8;
	}
	img->bytes_per_line = bpl;
	return img;
}

extern "C" int
XPutPixel(XImage *img, int x, int y, unsigned long p)
{
	if (img->bits_per_pixel == 32) {
		uint32_t *line = (uint32_t *)(img->data + y * img->bytes_per_line);
		line[x] = (uint32_t)p;
	} else {
		unsigned char *b = (unsigned char *)img->data + y * img->bytes_per_line
			+ (x >> 3);
		if (p & 1) *b |= 1 << (x & 7);
		else *b &= ~(1 << (x & 7));
	}
	return 1;
}

extern "C" unsigned long
XGetPixel(XImage *img, int x, int y)
{
	if (img->bits_per_pixel == 32)
		return ((uint32_t *)(img->data + y * img->bytes_per_line))[x];
	unsigned char b = ((unsigned char *)img->data)[y * img->bytes_per_line + (x >> 3)];
	return (b >> (x & 7)) & 1;
}

static QImage
ximage_to_qimage(XImage *img, GC gc)
{
	if (img->bits_per_pixel == 32) {
		QImage q = QImage((const uchar *)img->data, img->width, img->height,
				img->bytes_per_line, QImage::Format_RGB32).copy();
		/* X ignores the top byte, but some of xfig's image readers
		   (ghostscript output) leave it 0, and Qt copies RGB32 bits
		   unchanged: the pixels would become transparent */
		for (int y = 0; y < q.height(); ++y) {
			QRgb *line = reinterpret_cast<QRgb *>(q.scanLine(y));
			for (int x = 0; x < q.width(); ++x)
				line[x] |= 0xff000000u;
		}
		return q;
	}
	QImage q(img->width, img->height, QImage::Format_RGB32);
	QRgb fg = gc ? pix2rgb(gc->v.foreground) : 0xffffffffu;
	QRgb bg = gc ? pix2rgb(gc->v.background) : 0xff000000u;
	for (int y = 0; y < img->height; ++y) {
		QRgb *line = reinterpret_cast<QRgb *>(q.scanLine(y));
		for (int x = 0; x < img->width; ++x)
			line[x] = XGetPixel(img, x, y) ? fg : bg;
	}
	return q;
}

extern "C" int
XPutImage(Display *, Drawable d, GC gc, XImage *img, int sx, int sy, int dx,
		int dy, unsigned w, unsigned h)
{
	QImage q = ximage_to_qimage(img, gc);
	return paint(d, gc, [&](QPainter &p) {
		p.drawImage(QRectF(dx, dy, w, h), q, QRectF(sx, sy, w, h));
	});
}

extern "C" XImage *
XGetImage(Display *dpy, Drawable d, int x, int y, unsigned w, unsigned h,
		unsigned long, int format)
{
	DrawableRec *r = lookup(d);
	if (!r)
		return nullptr;
	qreal dpr = r->img.devicePixelRatio();
	QImage part = r->img.copy(QRect(x * dpr, y * dpr, w * dpr, h * dpr))
		.scaled(w, h).convertToFormat(QImage::Format_RGB32);
	char *data = (char *)malloc(part.sizeInBytes());
	memcpy(data, part.constBits(), part.sizeInBytes());
	XImage *img = XCreateImage(dpy, nullptr, 24, ZPixmap, 0, data, w, h, 32,
			part.bytesPerLine());
	(void)format;
	return img;
}

extern "C" int
XDestroyImage(XImage *img)
{
	free(img->data);
	free(img);
	return 1;
}

/* --------------------------------------------------------------- regions */

extern "C" Region XCreateRegion(void) { return new _XRegion; }

extern "C" Region
XPolygonRegion(XPoint *pts, int n, int rule)
{
	Region r = new _XRegion;
	QPolygon poly;
	for (int i = 0; i < n; ++i)
		poly << QPoint(pts[i].x, pts[i].y);
	r->r = QRegion(poly, rule == WindingRule ? Qt::WindingFill : Qt::OddEvenFill);
	return r;
}

extern "C" int
XSubtractRegion(Region a, Region b, Region out)
{
	out->r = a->r.subtracted(b->r);
	return 1;
}

extern "C" int
XUnionRectWithRegion(XRectangle *rect, Region src, Region out)
{
	out->r = src->r.united(QRect(rect->x, rect->y, rect->width, rect->height));
	return 1;
}

extern "C" int XDestroyRegion(Region r) { delete r; return 1; }

/* ---------------------------------------------------------------- colors */

static unsigned long
rgb2pix(unsigned short r, unsigned short g, unsigned short b)
{
	return ((unsigned long)(r >> 8) << 16) | ((g >> 8) << 8) | (b >> 8);
}

extern "C" Status
XAllocColor(Display *, Colormap, XColor *c)
{
	c->pixel = rgb2pix(c->red, c->green, c->blue);
	/* report back the color actually allocated */
	c->red = ((c->pixel >> 16) & 0xff) * 0x101;
	c->green = ((c->pixel >> 8) & 0xff) * 0x101;
	c->blue = (c->pixel & 0xff) * 0x101;
	return 1;
}

extern "C" Status
XAllocColorCells(Display *, Colormap, Bool, unsigned long *, unsigned,
		unsigned long *, unsigned)
{
	return 0;	/* TrueColor: no read/write cells */
}

static bool
parse_x11_color(const char *spec, QColor &c)
{
	QString s = QString::fromLatin1(spec).trimmed();
	if (s.startsWith('#')) {
		/* X allows #rgb, #rrggbb, #rrrgggbbb and #rrrrggggbbbb */
		c = QColor::fromString(s);
		return c.isValid();
	}
	if (s.startsWith("rgb:", Qt::CaseInsensitive)) {
		QStringList p = s.mid(4).split('/');
		if (p.size() != 3)
			return false;
		int v[3];
		for (int i = 0; i < 3; ++i) {
			bool ok;
			int n = p[i].toInt(&ok, 16);
			if (!ok) return false;
			int bits = 4 * p[i].size();
			v[i] = bits >= 8 ? n >> (bits - 8) : n * 255 / ((1 << bits) - 1);
		}
		c = QColor(v[0], v[1], v[2]);
		return true;
	}
	QString n = s.toLower().remove(' ');
	/* X11 "grayNN" / "greyNN" */
	if ((n.startsWith("gray") || n.startsWith("grey")) && n.size() > 4) {
		bool ok;
		int pct = n.mid(4).toInt(&ok);
		if (ok && pct >= 0 && pct <= 100) {
			int v = int(pct * 255 / 100.0 + 0.5);
			c = QColor(v, v, v);
			return true;
		}
	}
	/* X11 names with a trailing digit, e.g. "red3": approximate */
	static const double shade[] = {1.0, 1.0, 0.932, 0.804, 0.545};
	if (!n.isEmpty() && n.back().isDigit()) {
		int k = n.back().digitValue();
		QColor base = QColor::fromString(n.chopped(1));
		if (base.isValid() && k >= 1 && k <= 4) {
			c = QColor(base.red() * shade[k], base.green() * shade[k],
					base.blue() * shade[k]);
			return true;
		}
	}
	c = QColor::fromString(n);
	return c.isValid();
}

extern "C" Status
XParseColor(Display *, Colormap, const char *spec, XColor *xc)
{
	QColor c;
	if (!spec || !parse_x11_color(spec, c))
		return 0;
	xc->red = c.red() * 0x101;
	xc->green = c.green() * 0x101;
	xc->blue = c.blue() * 0x101;
	xc->flags = DoRed | DoGreen | DoBlue;
	return 1;
}

extern "C" Status
XLookupColor(Display *d, Colormap cm, const char *spec, XColor *exact, XColor *screen)
{
	if (!XParseColor(d, cm, spec, exact))
		return 0;
	*screen = *exact;
	XAllocColor(d, cm, screen);
	return 1;
}

extern "C" int
XQueryColor(Display *, Colormap, XColor *c)
{
	c->red = ((c->pixel >> 16) & 0xff) * 0x101;
	c->green = ((c->pixel >> 8) & 0xff) * 0x101;
	c->blue = (c->pixel & 0xff) * 0x101;
	c->flags = DoRed | DoGreen | DoBlue;
	return 1;
}

extern "C" int
XQueryColors(Display *d, Colormap cm, XColor *c, int n)
{
	for (int i = 0; i < n; ++i)
		XQueryColor(d, cm, &c[i]);
	return 1;
}

extern "C" int XStoreColor(Display *, Colormap, XColor *) { return 1; }
extern "C" int XStoreColors(Display *, Colormap, XColor *, int) { return 1; }
extern "C" int XFreeColors(Display *, Colormap, unsigned long *, int, unsigned long) { return 1; }

/* ------------------------------------------------------- X core fonts */

static std::vector<QFont> core_fonts;

static XFontStruct *
make_fontstruct(const QFont &f)
{
	core_fonts.push_back(f);
	QFontMetrics m(f);
	XFontStruct *fs = (XFontStruct *)calloc(1, sizeof(XFontStruct));
	fs->fid = core_fonts.size();	/* index + 1 */
	fs->ascent = m.ascent();
	fs->descent = m.descent();
	fs->min_char_or_byte2 = 0;
	fs->max_char_or_byte2 = 255;
	/* Qt's maxWidth() covers every glyph in the font, far wider than the
	   Latin-1 characters of an X core font: use a wide letter instead */
	int wide = m.horizontalAdvance(QLatin1Char('M'));
	fs->max_bounds.width = wide;
	fs->max_bounds.ascent = m.ascent();
	fs->max_bounds.descent = m.descent();
	fs->max_bounds.rbearing = wide;
	fs->min_bounds = fs->max_bounds;
	fs->min_bounds.width = m.averageCharWidth();
	return fs;
}

static const QFont &
core_font(Font fid)
{
	static QFont fallback = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
	if (fid >= 1 && fid <= core_fonts.size())
		return core_fonts[fid - 1];
	return fallback;
}

/* parse an XLFD such as -*-times-medium-r-normal--14-*-*-*-*-*-*-* */
extern "C" XFontStruct *
XLoadQueryFont(Display *, const char *name)
{
	QFont f = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
	QString n = QString::fromLatin1(name ? name : "fixed");
	/*
	 * xfig's user interface fonts ("fixed", 6x13, helvetica) become the
	 * system font, so that rulers and panels match the rest of the UI.
	 */
	if (n == "fixed" || n.startsWith("6x") || n.startsWith("7x") ||
			n.startsWith("8x") || n.startsWith("9x")) {
		f.setPixelSize(11);
	} else if (n.startsWith('-')) {
		QStringList p = n.split('-');
		/* p[0]="" foundry family weight slant setwidth style pixel point */
		if (p.size() > 2 && p[2] != "*" && p[2].compare("helvetica",
					Qt::CaseInsensitive) != 0 &&
				p[2].compare("fixed", Qt::CaseInsensitive) != 0)
			f.setFamily(p[2]);
		if (p.size() > 3)
			f.setBold(p[3] == "bold");
		if (p.size() > 4)
			f.setItalic(p[4] == "i" || p[4] == "o");
		bool ok = false;
		int px = p.size() > 7 ? p[7].toInt(&ok) : 0;
		if (ok && px > 0)
			f.setPixelSize(px);
		else if (p.size() > 8 && (px = p[8].toInt(&ok)) > 0 && ok)
			f.setPixelSize(px / 10);
		else
			f.setPixelSize(12);
	} else {
		f.setPixelSize(12);
	}
	return make_fontstruct(f);
}

QFont
xq_qfont(XFontStruct *fs)
{
	return core_font(fs ? fs->fid : 0);
}

extern "C" XFontStruct *
XQueryFont(Display *, XID fid)
{
	return make_fontstruct(core_font(fid));
}

extern "C" int XFreeFont(Display *, XFontStruct *fs) { free(fs); return 1; }

extern "C" int
XTextWidth(XFontStruct *fs, const char *s, int n)
{
	return QFontMetrics(core_font(fs->fid)).horizontalAdvance(
			QString::fromLatin1(s, n));
}

extern "C" int
XTextExtents(XFontStruct *fs, const char *s, int n, int *dir, int *asc,
		int *desc, XCharStruct *overall)
{
	QFontMetrics m(core_font(fs->fid));
	QString str = QString::fromLatin1(s, n);
	QRect br = m.boundingRect(str);
	if (dir) *dir = 0;
	if (asc) *asc = m.ascent();
	if (desc) *desc = m.descent();
	if (overall) {
		overall->width = m.horizontalAdvance(str);
		overall->ascent = -br.top();
		overall->descent = br.bottom();
		overall->lbearing = br.left();
		overall->rbearing = br.right();
	}
	return 1;
}

extern "C" char **XListFonts(Display *, const char *, int, int *count) { *count = 0; return nullptr; }
extern "C" int XFreeFontNames(char **) { return 1; }

static int
draw_string(Drawable d, GC gc, int x, int y, const char *s, int n, bool image)
{
	return paint(d, gc, [&](QPainter &p) {
		QFont f = core_font(gc->v.font);
		p.setFont(f);
		QString str = QString::fromLatin1(s, n);
		if (image) {
			QFontMetrics m(f);
			p.fillRect(QRect(x, y - m.ascent(), m.horizontalAdvance(str),
				m.height()), QColor::fromRgb(pix2rgb(gc->v.background)));
		}
		p.setPen(QColor::fromRgb(pix2rgb(gc->v.foreground)));
		p.drawText(x, y, str);
	}, true);
}

extern "C" int
XDrawString(Display *, Drawable d, GC gc, int x, int y, const char *s, int n)
{
	return draw_string(d, gc, x, y, s, n, false);
}

extern "C" int
XDrawImageString(Display *, Drawable d, GC gc, int x, int y, const char *s, int n)
{
	return draw_string(d, gc, x, y, s, n, true);
}

/* ------------------------------------------------------------- misc Xlib */

extern "C" int XDefineCursor(Display *, Window w, Cursor c) { if (cursor_cb) cursor_cb(w, c); return 1; }
extern "C" int XUndefineCursor(Display *, Window w) { if (cursor_cb) cursor_cb(w, None); return 1; }
extern "C" int XBell(Display *, int) { QApplication::beep(); return 1; }
extern "C" int XFlush(Display *) { return 1; }
extern "C" int XSync(Display *, Bool) { return 1; }
extern "C" int XFree(void *p) { free(p); return 1; }
static XErrorHandler err_handler;
static XIOErrorHandler ioerr_handler;
extern "C" XErrorHandler
XSetErrorHandler(XErrorHandler h)
{
	XErrorHandler old = err_handler;
	err_handler = h;
	return old;
}
extern "C" XIOErrorHandler
XSetIOErrorHandler(XIOErrorHandler h)
{
	XIOErrorHandler old = ioerr_handler;
	ioerr_handler = h;
	return old;
}
extern "C" int
XGetErrorText(Display *, int code, char *buf, int len)
{
	snprintf(buf, len, "X error %d", code);
	return 1;
}
extern "C" int
XGetErrorDatabaseText(Display *, const char *, const char *, const char *def,
		char *buf, int len)
{
	snprintf(buf, len, "%s", def ? def : "");
	return 1;
}
extern "C" XVisualInfo *
XGetVisualInfo(Display *, long, XVisualInfo *, int *n)
{
	*n = 0;
	return nullptr;
}
extern "C" int XWarpPointer(Display *, Window, Window, int, int, unsigned, unsigned, int, int) { return 1; }

/* ------------------------------------------------------------------- Xft */

static FT_Library ft_lib;

struct FaceRec {
	FT_Face	face;
	QHash<FT_UInt, QPainterPath> outlines;	/* font units, y up */
	QHash<FT_UInt, qreal> advances;		/* font units */
};

static QHash<QString, FaceRec *> faces;

struct XftFontPriv {
	FaceRec	*face;
	double	pixelsize;
	double	scale;		/* pixels per font unit */
	FcMatrix m;
};

static FaceRec *
open_face(const char *file, int index)
{
	QString key = QString::fromUtf8(file) + "#" + QString::number(index);
	auto it = faces.find(key);
	if (it != faces.end())
		return it.value();
	if (!ft_lib && FT_Init_FreeType(&ft_lib))
		return nullptr;
	FT_Face face;
	if (FT_New_Face(ft_lib, file, index, &face))
		return nullptr;
	if (!face->charmap && face->num_charmaps > 0)
		FT_Set_Charmap(face, face->charmaps[0]);
	FaceRec *r = new FaceRec;
	r->face = face;
	faces.insert(key, r);
	return r;
}

struct OutlineCtx { QPainterPath *path; };

static int ft_move(const FT_Vector *to, void *u)
{
	static_cast<OutlineCtx *>(u)->path->moveTo(to->x, to->y);
	return 0;
}
static int ft_line(const FT_Vector *to, void *u)
{
	static_cast<OutlineCtx *>(u)->path->lineTo(to->x, to->y);
	return 0;
}
static int ft_conic(const FT_Vector *c, const FT_Vector *to, void *u)
{
	static_cast<OutlineCtx *>(u)->path->quadTo(c->x, c->y, to->x, to->y);
	return 0;
}
static int ft_cubic(const FT_Vector *c1, const FT_Vector *c2, const FT_Vector *to, void *u)
{
	static_cast<OutlineCtx *>(u)->path->cubicTo(c1->x, c1->y, c2->x, c2->y, to->x, to->y);
	return 0;
}

static void
load_glyph(FaceRec *f, FT_UInt g)
{
	if (f->advances.contains(g))
		return;
	QPainterPath path;
	qreal adv = 0;
	if (!FT_Load_Glyph(f->face, g, FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP |
				FT_LOAD_NO_HINTING)) {
		FT_GlyphSlot slot = f->face->glyph;
		adv = slot->advance.x;
		if (slot->format == FT_GLYPH_FORMAT_OUTLINE) {
			FT_Outline_Funcs funcs = {ft_move, ft_line, ft_conic, ft_cubic, 0, 0};
			OutlineCtx ctx{&path};
			FT_Outline_Decompose(&slot->outline, &funcs, &ctx);
			path.closeSubpath();
			path.setFillRule(slot->outline.flags & FT_OUTLINE_EVEN_ODD_FILL ?
					Qt::OddEvenFill : Qt::WindingFill);
		}
	}
	f->outlines.insert(g, path);
	f->advances.insert(g, adv);
}

static inline XftFontPriv *priv(XftFont *f) { return static_cast<XftFontPriv *>(f->qfont); }

/* font units (y up) to pixel offsets (y down), applying the font matrix */
static QTransform
glyph_transform(XftFontPriv *p)
{
	const FcMatrix &m = p->m;
	double s = p->scale;
	/* x' = s*(xx*x + xy*y), y' = -s*(yx*x + yy*y) */
	return QTransform(s * m.xx, -s * m.yx, s * m.xy, -s * m.yy, 0, 0);
}

static QPointF
advance_vector(XftFontPriv *p, FT_UInt g)
{
	qreal a = p->face->advances.value(g);
	QPointF v = glyph_transform(p).map(QPointF(a, 0));
	return QPointF(std::round(v.x()), std::round(v.y()));
}

extern "C" FcPattern *
XftNameParse(const char *name)
{
	return FcNameParse((const FcChar8 *)name);
}

extern "C" FcPattern *
XftFontMatch(Display *, int, const FcPattern *pattern, FcResult *result)
{
	FcPattern *p = FcPatternDuplicate(pattern);
	FcConfigSubstitute(nullptr, p, FcMatchPattern);
	FcDefaultSubstitute(p);
	FcPattern *m = FcFontMatch(nullptr, p, result);
	FcPatternDestroy(p);
	return m;
}

extern "C" XftFont *
XftFontOpenPattern(Display *, FcPattern *pattern)
{
	if (!pattern)
		return nullptr;
	FcChar8 *file = nullptr;
	int index = 0;
	if (FcPatternGetString(pattern, FC_FILE, 0, &file) != FcResultMatch)
		return nullptr;
	FcPatternGetInteger(pattern, FC_INDEX, 0, &index);
	FaceRec *face = open_face((const char *)file, index);
	if (!face)
		return nullptr;
	double px = 0;
	if (FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0, &px) != FcResultMatch) {
		double size = 12, dpi = 75;
		FcPatternGetDouble(pattern, FC_SIZE, 0, &size);
		FcPatternGetDouble(pattern, FC_DPI, 0, &dpi);
		px = size * dpi / 72.0;
	}
	XftFontPriv *p = new XftFontPriv;
	p->face = face;
	p->pixelsize = px;
	int upem = face->face->units_per_EM ? face->face->units_per_EM : 1000;
	p->scale = px / upem;
	FcMatrix *mat;
	if (FcPatternGetMatrix(pattern, FC_MATRIX, 0, &mat) == FcResultMatch)
		p->m = *mat;
	else
		FcMatrixInit(&p->m);

	XftFont *f = (XftFont *)calloc(1, sizeof(XftFont));
	FT_Face ff = face->face;
	f->ascent = (int)std::ceil(ff->ascender * p->scale);
	f->descent = (int)std::ceil(-ff->descender * p->scale);
	f->height = (int)std::ceil(ff->height * p->scale);
	if (f->height < f->ascent + f->descent)
		f->height = f->ascent + f->descent;
	f->max_advance_width = (int)std::ceil(ff->max_advance_width * p->scale);
	f->pattern = pattern;
	f->qfont = p;
	return f;
}

extern "C" XftFont *
XftFontOpenName(Display *d, int screen, const char *name)
{
	FcPattern *pat = XftNameParse(name);
	FcResult res;
	FcPattern *m = XftFontMatch(d, screen, pat, &res);
	FcPatternDestroy(pat);
	return XftFontOpenPattern(d, m);
}

extern "C" XftFont *
XftFontCopy(Display *d, XftFont *f)
{
	XftFont *n = (XftFont *)malloc(sizeof(XftFont));
	*n = *f;
	n->pattern = FcPatternDuplicate(f->pattern);
	n->qfont = new XftFontPriv(*priv(f));
	(void)d;
	return n;
}

extern "C" void
XftFontClose(Display *, XftFont *f)
{
	if (!f)
		return;
	delete priv(f);
	FcPatternDestroy(f->pattern);
	free(f);
}

extern "C" FT_UInt
XftCharIndex(Display *, XftFont *f, FcChar32 ucs4)
{
	return FT_Get_Char_Index(priv(f)->face->face, ucs4);
}

extern "C" FcBool
XftCharExists(Display *d, XftFont *f, FcChar32 ucs4)
{
	return XftCharIndex(d, f, ucs4) != 0;
}

static std::vector<FT_UInt>
utf8_glyphs(XftFont *f, const FcChar8 *s, int len)
{
	std::vector<FT_UInt> glyphs;
	FcChar32 c;
	while (len > 0) {
		int n = FcUtf8ToUcs4(s, &c, len);
		if (n <= 0)
			break;
		glyphs.push_back(FT_Get_Char_Index(priv(f)->face->face, c));
		s += n;
		len -= n;
	}
	return glyphs;
}

extern "C" void
XftGlyphExtents(Display *, XftFont *f, const FT_UInt *glyphs, int n, XGlyphInfo *ext)
{
	XftFontPriv *p = priv(f);
	QTransform t = glyph_transform(p);
	QPointF pen(0, 0);
	QRectF ink;
	for (int i = 0; i < n; ++i) {
		load_glyph(p->face, glyphs[i]);
		QRectF b = t.map(p->face->outlines.value(glyphs[i])).boundingRect();
		if (!b.isEmpty())
			ink |= b.translated(pen);
		pen += advance_vector(p, glyphs[i]);
	}
	if (ink.isEmpty()) {
		ext->x = ext->y = 0;
		ext->width = ext->height = 0;
	} else {
		int l = (int)std::floor(ink.left()), t2 = (int)std::floor(ink.top());
		int r = (int)std::ceil(ink.right()), b = (int)std::ceil(ink.bottom());
		ext->x = -l;
		ext->y = -t2;
		ext->width = r - l;
		ext->height = b - t2;
	}
	ext->xOff = (short)pen.x();
	ext->yOff = (short)pen.y();
}

extern "C" void
XftTextExtentsUtf8(Display *d, XftFont *f, const FcChar8 *s, int len, XGlyphInfo *ext)
{
	std::vector<FT_UInt> g = utf8_glyphs(f, s, len);
	XftGlyphExtents(d, f, g.data(), (int)g.size(), ext);
}

extern "C" void
XftTextExtents8(Display *d, XftFont *f, const FcChar8 *s, int len, XGlyphInfo *ext)
{
	std::vector<FT_UInt> g;
	for (int i = 0; i < len; ++i)
		g.push_back(XftCharIndex(d, f, s[i]));
	XftGlyphExtents(d, f, g.data(), (int)g.size(), ext);
}

struct _XftDraw {
	Drawable d;
	bool	has_clip = false;
	QRegion	clip;
};

extern "C" XftDraw *
XftDrawCreate(Display *, Drawable d, Visual *, Colormap)
{
	XftDraw *x = new _XftDraw;
	x->d = d;
	return x;
}

extern "C" void XftDrawDestroy(XftDraw *x) { delete x; }
extern "C" Drawable XftDrawDrawable(XftDraw *x) { return x->d; }
extern "C" void XftDrawChange(XftDraw *x, Drawable d) { x->d = d; }

extern "C" Bool
XftDrawSetClipRectangles(XftDraw *x, int xo, int yo, const XRectangle *r, int n)
{
	QRegion reg;
	for (int i = 0; i < n; ++i)
		reg += QRect(r[i].x + xo, r[i].y + yo, r[i].width, r[i].height);
	x->clip = reg;
	x->has_clip = true;
	return True;
}

extern "C" Bool
XftDrawSetClip(XftDraw *x, Region r)
{
	x->has_clip = r != nullptr;
	if (r)
		x->clip = r->r;
	return True;
}

static QColor
xft_qcolor(const XftColor *c)
{
	return QColor::fromRgb(c->color.red >> 8, c->color.green >> 8,
			c->color.blue >> 8, c->color.alpha >> 8);
}

extern "C" void
XftDrawGlyphs(XftDraw *x, const XftColor *color, XftFont *f, int ox, int oy,
		const FT_UInt *glyphs, int n)
{
	DrawableRec *r = lookup(x->d);
	if (!r || !f)
		return;
	XftFontPriv *p = priv(f);
	QTransform t = glyph_transform(p);
	QPainterPath all;
	QPointF pen(ox, oy);
	for (int i = 0; i < n; ++i) {
		load_glyph(p->face, glyphs[i]);
		all.addPath(t.map(p->face->outlines.value(glyphs[i])).translated(pen));
		pen += advance_vector(p, glyphs[i]);
	}
	QPainter painter(&r->img);
	painter.setRenderHint(QPainter::Antialiasing, true);
	if (x->has_clip)
		painter.setClipRegion(x->clip);
	painter.fillPath(all, xft_qcolor(color));
	painter.end();
	damage(x->d, r);
}

extern "C" void
XftDrawStringUtf8(XftDraw *x, const XftColor *color, XftFont *f, int ox, int oy,
		const FcChar8 *s, int len)
{
	std::vector<FT_UInt> g = utf8_glyphs(f, s, len);
	XftDrawGlyphs(x, color, f, ox, oy, g.data(), (int)g.size());
}

extern "C" void
XftDrawString8(XftDraw *x, const XftColor *color, XftFont *f, int ox, int oy,
		const FcChar8 *s, int len)
{
	std::vector<FT_UInt> g;
	for (int i = 0; i < len; ++i)
		g.push_back(XftCharIndex(nullptr, f, s[i]));
	XftDrawGlyphs(x, color, f, ox, oy, g.data(), (int)g.size());
}

extern "C" void
XftDrawRect(XftDraw *x, const XftColor *color, int rx, int ry, unsigned w, unsigned h)
{
	DrawableRec *r = lookup(x->d);
	if (!r)
		return;
	QPainter painter(&r->img);
	if (x->has_clip)
		painter.setClipRegion(x->clip);
	painter.fillRect(QRect(rx, ry, w, h), xft_qcolor(color));
	painter.end();
	damage(x->d, r);
}

extern "C" Bool
XftColorAllocValue(Display *, Visual *, Colormap, const XRenderColor *c, XftColor *out)
{
	out->color = *c;
	out->pixel = rgb2pix(c->red, c->green, c->blue);
	return True;
}

extern "C" Bool
XftColorAllocName(Display *d, const Visual *, Colormap cm, const char *name, XftColor *out)
{
	XColor xc;
	if (!XParseColor(d, cm, name, &xc))
		return False;
	out->color.red = xc.red;
	out->color.green = xc.green;
	out->color.blue = xc.blue;
	out->color.alpha = 0xffff;
	out->pixel = rgb2pix(xc.red, xc.green, xc.blue);
	return True;
}

extern "C" void XftColorFree(Display *, Visual *, Colormap, XftColor *) {}
