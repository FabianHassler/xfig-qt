/*
 * xpm.h - the part of libXpm that xfig uses, implemented in shim/xpm.c.
 */
#ifndef XQ_XPM_H
#define XQ_XPM_H

#include "xshim.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XpmSuccess	0
#define XpmOpenFailed	-1
#define XpmFileInvalid	-2
#define XpmNoMemory	-3
#define XpmColorFailed	-4

/* XpmAttributes.valuemask */
#define XpmColormap	(1L << 1)
#define XpmSize		(1L << 3)
#define XpmReturnPixels	(1L << 9)

typedef struct {
	char	*string;	/* the characters standing for the color */
	char	*symbolic;
	char	*m_color;
	char	*g4_color;
	char	*g_color;
	char	*c_color;
} XpmColor;

typedef struct {
	unsigned int	width, height;
	unsigned int	cpp;		/* characters per pixel */
	unsigned int	ncolors;
	XpmColor	*colorTable;
	unsigned int	*data;		/* width * height color indices */
} XpmImage;

typedef struct {
	unsigned long	valuemask;
	Visual		*visual;
	Colormap	colormap;
	unsigned int	depth;
	unsigned int	width, height;
	unsigned int	x_hotspot, y_hotspot;
	unsigned int	cpp;
	Pixel		*pixels;
	unsigned int	npixels;
} XpmAttributes;

int	XpmReadFileToXpmImage(const char *file, XpmImage *image, void *info);
void	XpmFreeXpmImage(XpmImage *image);
int	XpmCreatePixmapFromData(Display *d, Drawable dr, char **data,
		Pixmap *pixmap, Pixmap *shapemask, XpmAttributes *attr);

#ifdef __cplusplus
}
#endif

#endif /* XQ_XPM_H */
