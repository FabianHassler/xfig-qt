/*
 * xpm.c - read XPM 3 images, for xfig's XPM picture objects, its splash
 * screen and its icon.  Only what xfig uses of libXpm.
 */
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "X11/xpm.h"

/* the string literals of a C file, in order */
static char **
c_strings(const char *text, size_t len, int *count)
{
	char	**v = NULL;
	int	n = 0, cap = 0;
	size_t	i = 0;

	while (i < len) {
		if (text[i] == '/' && i + 1 < len && text[i + 1] == '*') {
			const char *e = strstr(text + i + 2, "*/");
			i = e ? (size_t)(e - text) + 2 : len;
			continue;
		}
		if (text[i] == '/' && i + 1 < len && text[i + 1] == '/') {
			while (i < len && text[i] != '\n')
				i++;
			continue;
		}
		if (text[i] != '"') {
			i++;
			continue;
		}
		size_t start = ++i;
		while (i < len && text[i] != '"')
			i += text[i] == '\\' ? 2 : 1;
		if (n == cap) {
			cap = cap ? 2 * cap : 64;
			char **nv = realloc(v, cap * sizeof *v);
			if (!nv)
				break;
			v = nv;
		}
		size_t l = (i < len ? i : len) - start;
		v[n] = malloc(l + 1);
		memcpy(v[n], text + start, l);
		v[n++][l] = '\0';
		i++;
	}
	*count = n;
	return v;
}

static char *
dup_value(const char *s, size_t n)
{
	char *d = malloc(n + 1);
	if (d) {
		memcpy(d, s, n);
		d[n] = '\0';
	}
	return d;
}

static int
is_key(const char *t, size_t n)
{
	return (n == 1 && strchr("cmgs", *t)) || (n == 2 && !strncmp(t, "g4", 2));
}

/* "<chars> c #ff0000 m black s name": a value may contain blanks
   ("light blue") and runs to the next key */
static void
parse_color(const char *line, unsigned cpp, XpmColor *c)
{
	memset(c, 0, sizeof *c);
	c->string = dup_value(line, strnlen(line, cpp));
	const char *p = line + strnlen(line, cpp);
	char **slot = NULL;
	const char *vstart = NULL, *vend = NULL;

	for (;;) {
		while (*p && isspace((unsigned char)*p))
			p++;
		const char *t = p;
		while (*p && !isspace((unsigned char)*p))
			p++;
		size_t tn = p - t;
		if (tn == 0 || (is_key(t, tn) && (!slot || vstart))) {
			if (slot && vstart)
				*slot = dup_value(vstart, vend - vstart);
			if (tn == 0)
				break;
			slot = *t == 'c' ? &c->c_color :
				*t == 'm' ? &c->m_color :
				*t == 's' ? &c->symbolic :
				tn == 2 ? &c->g4_color : &c->g_color;
			vstart = vend = NULL;
		} else if (slot) {
			if (!vstart)
				vstart = t;
			vend = p;
		}
	}
}

static int
parse_xpm(char **s, int n, XpmImage *img)
{
	unsigned int	w, h, nc, cpp;

	memset(img, 0, sizeof *img);
	if (n < 1 || sscanf(s[0], "%u %u %u %u", &w, &h, &nc, &cpp) != 4 ||
			w == 0 || h == 0 || nc == 0 || cpp == 0 || cpp > 8 ||
			(unsigned)n < 1 + nc + h)
		return XpmFileInvalid;
	img->width = w;
	img->height = h;
	img->ncolors = nc;
	img->cpp = cpp;
	img->colorTable = calloc(nc, sizeof(XpmColor));
	img->data = calloc((size_t)w * h, sizeof(unsigned int));
	if (!img->colorTable || !img->data) {
		XpmFreeXpmImage(img);
		return XpmNoMemory;
	}
	for (unsigned i = 0; i < nc; ++i)
		parse_color(s[1 + i], cpp, &img->colorTable[i]);

	/* look pixels up directly for 1 or 2 characters per pixel */
	int *direct = NULL;
	if (cpp <= 2) {
		direct = malloc((cpp == 1 ? 256 : 65536) * sizeof(int));
		if (direct) {
			memset(direct, 0, (cpp == 1 ? 256 : 65536) * sizeof(int));
			for (unsigned i = nc; i-- > 0; ) {
				const unsigned char *k =
					(const unsigned char *)img->colorTable[i].string;
				direct[cpp == 1 ? k[0] : k[0] << 8 | k[1]] = i;
			}
		}
	}
	for (unsigned y = 0; y < h; ++y) {
		const char *row = s[1 + nc + y];
		size_t rl = strlen(row);
		for (unsigned x = 0; x < w && (x + 1) * cpp <= rl; ++x) {
			const char *px = row + x * cpp;
			unsigned idx = 0;
			if (direct) {
				const unsigned char *u = (const unsigned char *)px;
				idx = direct[cpp == 1 ? u[0] : u[0] << 8 | u[1]];
			} else {
				for (unsigned i = 0; i < nc; ++i)
					if (!strncmp(img->colorTable[i].string, px, cpp)) {
						idx = i;
						break;
					}
			}
			img->data[(size_t)y * w + x] = idx;
		}
	}
	free(direct);
	return XpmSuccess;
}

int
XpmReadFileToXpmImage(const char *file, XpmImage *image, void *info)
{
	(void)info;
	FILE *f = fopen(file, "rb");
	if (!f)
		return XpmOpenFailed;
	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *text = len > 0 ? malloc(len + 1) : NULL;
	if (!text || fread(text, 1, len, f) != (size_t)len) {
		free(text);
		fclose(f);
		return len > 0 ? XpmNoMemory : XpmFileInvalid;
	}
	fclose(f);
	text[len] = '\0';

	int n;
	char **s = c_strings(text, len, &n);
	free(text);
	int status = parse_xpm(s, n, image);
	for (int i = 0; i < n; ++i)
		free(s[i]);
	free(s);
	return status;
}

void
XpmFreeXpmImage(XpmImage *image)
{
	if (image->colorTable) {
		for (unsigned i = 0; i < image->ncolors; ++i) {
			XpmColor *c = &image->colorTable[i];
			free(c->string);
			free(c->symbolic);
			free(c->m_color);
			free(c->g4_color);
			free(c->g_color);
			free(c->c_color);
		}
		free(image->colorTable);
	}
	free(image->data);
	memset(image, 0, sizeof *image);
}

/* data[] is the array of strings in an XPM file */
int
XpmCreatePixmapFromData(Display *d, Drawable dr, char **data, Pixmap *pixmap,
		Pixmap *shapemask, XpmAttributes *attr)
{
	XpmImage	img;
	unsigned int	n = 0, w, h, nc, cpp;

	if (!data || sscanf(data[0], "%u %u %u %u", &w, &h, &nc, &cpp) != 4)
		return XpmFileInvalid;
	n = 1 + nc + h;
	int status = parse_xpm(data, n, &img);
	if (status != XpmSuccess)
		return status;

	unsigned long *pix = malloc(img.ncolors * sizeof *pix);
	uint32_t *bits = malloc((size_t)img.width * img.height * 4);
	if (!pix || !bits) {
		free(pix);
		free(bits);
		XpmFreeXpmImage(&img);
		return XpmNoMemory;
	}
	Colormap cm = attr && (attr->valuemask & XpmColormap) ? attr->colormap : 0;
	for (unsigned i = 0; i < img.ncolors; ++i) {
		XpmColor *c = &img.colorTable[i];
		const char *name = c->c_color ? c->c_color :
			c->g_color ? c->g_color :
			c->g4_color ? c->g4_color : c->m_color;
		XColor xc;
		/* "None" (transparent) and unknown colors become white */
		if (!name || !XParseColor(d, cm, name, &xc))
			xc.red = xc.green = xc.blue = 0xffff;
		XAllocColor(d, cm, &xc);
		pix[i] = xc.pixel;
	}
	for (size_t i = 0; i < (size_t)img.width * img.height; ++i)
		bits[i] = (uint32_t)pix[img.data[i]];

	XImage *xi = XCreateImage(d, NULL, 24, ZPixmap, 0, (char *)bits,
			img.width, img.height, 32, 0);
	*pixmap = XCreatePixmap(d, dr, img.width, img.height, 24);
	GC gc = XCreateGC(d, *pixmap, 0, NULL);
	XPutImage(d, *pixmap, gc, xi, 0, 0, 0, 0, img.width, img.height);
	XFreeGC(d, gc);
	XDestroyImage(xi);
	if (shapemask)
		*shapemask = None;
	if (attr) {
		attr->width = img.width;
		attr->height = img.height;
		attr->cpp = img.cpp;
		/* the colors need not be freed */
		attr->pixels = NULL;
		attr->npixels = 0;
	}
	free(pix);
	XpmFreeXpmImage(&img);
	return XpmSuccess;
}
