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

/*
 * Some of the following code is extracted from giftopnm.c,
 * from the netpbm package
 */

/* +-------------------------------------------------------------------+ */
/* | Copyright 1990, David Koblas.                                     | */
/* |   Permission to use, copy, modify, and distribute this software   | */
/* |   and its documentation for any purpose and without fee is hereby | */
/* |   granted, provided that the above copyright notice appear in all | */
/* |   copies and that both that copyright notice and this permission  | */
/* |   notice appear in supporting documentation.  This software is    | */
/* |   provided "as is" without express or implied warranty.           | */
/* +-------------------------------------------------------------------+ */

#ifdef HAVE_CONFIG_H
#include "config.h"		/* restrict */
#endif

#include <X11/Intrinsic.h>	/* Boolean */
#include <X11/Xlib.h>		/* True, False */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>		/* mkstemp */
#include <string.h>		/* mkstemp */
#include <unistd.h>		/* close, unlink */

#include "resources.h"		/* PATH_MAX */
#include "object.h"
#include "f_picobj.h"		/* init_stream() */
#include "f_readpcx.h"
#include "u_colors.h"
#include "u_spawn.h"
#include "w_msgpanel.h"


static Boolean	ReadColorMap(FILE *fd, unsigned int number, struct Cmap *cmap);
static Boolean	DoGIFextension(FILE *fd, int label);
static int	GetDataBlock(FILE *fd, unsigned char *buf);

#define LOCALCOLORMAP		0x80
#define	ReadOK(file,buffer,len)	\
	    (fread((void *)buffer, (size_t)len, (size_t)1, (FILE *)file) != 0)
#define BitSet(byte, bit)	(((byte) & (bit)) == (bit))

#define LM_to_uint(a,b)		(((b)<<8)|(a))

struct {
	unsigned int	Width;
	unsigned int	Height;
	struct Cmap	ColorMap[MAX_COLORMAP_SIZE];
	unsigned int	BitPixel;
	unsigned int	ColorResolution;
	unsigned int	Background;
	unsigned int	AspectRatio;
} GifScreen;

struct {
	int	transparent;
	int	delayTime;
	int	inputFlag;
	int	disposal;
} Gif89 = { -1, -1, -1, 0 };

/* return codes:  PicSuccess (1) : success
		  FileInvalid (-2) : invalid file
*/


int
read_gif(F_pic *pic, struct xfig_stream *restrict pic_stream)
{
	char		buf[BUFSIZ];
	char		*one[5];
	char		*two[3];
	struct Cmap	localColorMap[MAX_COLORMAP_SIZE];
	int		i, mid, stat;
	int		useGlobalColormap;
	unsigned int	bitPixel, red, green, blue;
	unsigned char	c;
	char		version[4];
	/* the contents of this stream is copied to pic_stream, and closed
	   outside of this function; therefore, static */
	static struct xfig_stream	pcx;

	if (!rewind_stream(pic_stream))
		return FileInvalid;

	/*
	 * Set up the command to convert gif to pcx.
	 */

	if (spawn_exists("giftopnm", "-version") &&
			spawn_exists("ppmtopcx", "-version")) {
		/* command: giftopnm -quiet | ppmtopcx -quiet */
		one[0] = "giftopnm";
		two[0] = "ppmtopcx";
		one[1] = two[1] = "-quiet";
		one[2] = two[2] = NULL;

	} else if (spawn_exists("convert", "-version")) {
		/* command: convert - pcx:- */
		one[0] = "convert";
		one[1] = "-";
		one[2] = "pcx:-";
		one[3] = NULL;
		two[0] = NULL;

	} else if (spawn_exists("gm", "-version")) {
		/* command: gm convert - pcx:- */
		one[0] = "gm";
		one[1] = "convert";
		one[2] = "-";
		one[3] = "pcx:-";
		one[4] = NULL;
		two[0] = NULL;

	} else {
		file_msg("Cannot read gif files.");
		file_msg("To read gif files, install either the netpbm, or the "
				"imagemagick, or the graphicsmagick package.");
		return FileInvalid;
	}

	/*
	 * Read transparency information.
	 */

	/* first read header to look for any transparent color extension */
	if (!ReadOK(pic_stream->fp, buf, 6)) {
		return FileInvalid;
	}

	if (strncmp(buf, "GIF", 3) != 0) {
		return FileInvalid;
	}

	strncpy(version, buf + 3, 3);
	version[3] = '\0';

	if ((strcmp(version, "87a") != 0) && (strcmp(version, "89a") != 0)) {
		file_msg("Unknown GIF version %s", version);
		return FileInvalid;
	}

	if (!ReadOK(pic_stream->fp, buf, 7)) {
		return FileInvalid;	/* failed to read screen descriptor */
	}

	GifScreen.Width           = LM_to_uint(buf[0],buf[1]);
	GifScreen.Height          = LM_to_uint(buf[2],buf[3]);
	GifScreen.BitPixel        = 2<<(buf[4]&0x07);
	GifScreen.ColorResolution = (((((int)buf[4])&0x70)>>3)+1);
	GifScreen.Background      = (unsigned int) buf[5];
	GifScreen.AspectRatio     = (unsigned int) buf[6];

	if (BitSet(buf[4], LOCALCOLORMAP)) {	/* Global Colormap */
		if (!ReadColorMap(pic_stream->fp, GifScreen.BitPixel,
					GifScreen.ColorMap)) {
			return FileInvalid;  /* error reading global colormap */
		}
	}

	if (GifScreen.AspectRatio != 0 && GifScreen.AspectRatio != 49) {
		if (appres.DEBUG)
			fprintf(stderr, "warning - non-square pixels\n");
	}

	/* assume no transparent color for now */
	Gif89.transparent = TRANSP_NONE;

	/* read the full header to get any transparency information */
	for (;;) {
		if (!ReadOK(pic_stream->fp, &c, 1)) {
			return FileInvalid;	/* EOF / read error on image data */
		}

		if (c == ';') {			/* GIF terminator, finish up */
			return PicSuccess;	/* all done */
		}

		if (c == '!') {			/* Extension */
			if (!ReadOK(pic_stream->fp, &c, 1))
				file_msg("GIF read error on extension "
						"function code");
			(void)DoGIFextension(pic_stream->fp, c);
			continue;
		}

		if (c != ',') {			/* Not a valid start character*/
			continue;
		}

		if (!ReadOK(pic_stream->fp, buf, 9)) {
			/* couldn't read left/top/width/height */
			return FileInvalid;
		}

		useGlobalColormap = !BitSet(buf[8], LOCALCOLORMAP);

		bitPixel = 1<<((buf[8]&0x07)+1);

		if (!useGlobalColormap) {
			if (!ReadColorMap(pic_stream->fp, bitPixel,
						localColorMap)) {
				file_msg("error reading local GIF colormap" );
				return PicSuccess;
			}
		}
		break;			/* image starts here, header is done */
	}

	/* save transparent indicator */
	pic->pic_cache->transp = Gif89.transparent;


	/*
	 * Call the conversion program, or pipeline.
	 */

	/*
	 * Construct a struct xfig_stream because read_pcx() expects one.
	 * Tell read_pcx() that the FILE pointer is positioned at the
	 * start (*name == '\0'). Close this stream and swap in our new stream
	 * to the one passed to us, which is then closed by the caller.
	 */
	init_stream(&pcx);
	pcx.name[0] = '\0';

	rewind_stream(pic_stream);
	if (two[0]) {
		/*
		 * In which direction to set up the pipeline?
		 * If from the end to the beginning, errors during setup are
		 * well separated from error during data processing. In the
		 * other direction, the first process might already have
		 * finished working, and data is in the buffers. The first
		 * element in the pipeline is already active, go forward.
		 */
		mid = spawn_popen_fd(one, NULL, "r", fileno(pic_stream->fp));
		i = spawn_popen_fd(two, NULL, "r", mid);
	} else {
		i = spawn_popen_fd(one, NULL, "r", fileno(pic_stream->fp));
	}

	pcx.fp = fdopen(i, "r");

	stat = read_pcx(pic, &pcx);
	close_stream(pic_stream);
	free_stream(pic_stream);

	if (two[0])
		spawn_pclose(mid);

	init_stream(pic_stream);
	pic_stream->fp = pcx.fp;
	/* set uncompress to anything, not NULL, to indicate it is a pipe */
	pic_stream->uncompress = &pic_stream->name;


	/*
	 * Process the transparent color information.
	 */

	pic->pic_cache->subtype = T_PIC_GIF;
	/* now match original transparent colortable index with possibly new
	   colortable from ppmtopcx */
	if (pic->pic_cache->transp != TRANSP_NONE) {
		if (useGlobalColormap) {
			red = GifScreen.ColorMap[pic->pic_cache->transp].red;
			green = GifScreen.ColorMap[pic->pic_cache->transp].green;
			blue = GifScreen.ColorMap[pic->pic_cache->transp].blue;
		} else {
			red = localColorMap[pic->pic_cache->transp].red;
			green = localColorMap[pic->pic_cache->transp].green;
			blue = localColorMap[pic->pic_cache->transp].blue;
		}
		for (i = 0; i < pic->pic_cache->numcols; ++i) {
			if (pic->pic_cache->cmap[i].red == red &&
					pic->pic_cache->cmap[i].green == green &&
					pic->pic_cache->cmap[i].blue == blue)
				break;
		}
		if (i < pic->pic_cache->numcols)
			pic->pic_cache->transp = i;
	}

	return stat;
}

static Boolean
ReadColorMap(FILE *fd, unsigned int number, struct Cmap *cmap)
{
	unsigned int	i;
	unsigned char	rgb[3];

	for (i = 0; i < number; ++i) {
		if (! ReadOK(fd, rgb, sizeof(rgb))) {
			file_msg("bad GIF colormap" );
			return False;
		}
		cmap[i].red   = rgb[0];
		cmap[i].green = rgb[1];
		cmap[i].blue  = rgb[2];
	}
	return True;
}

static Boolean
DoGIFextension(FILE *fd, int label)
{
	static unsigned char	buf[256];
	char		*str;

	switch (label) {
	case 0x01:		/* Plain Text Extension */
		str = "Plain Text Extension";
		break;
	case 0xff:		/* Application Extension */
		str = "Application Extension";
		break;
	case 0xfe:		/* Comment Extension */
		str = "Comment Extension";
		while (GetDataBlock(fd, buf) != 0) {
			; /* GIF comment */
		}
		return False;
	case 0xf9:		/* Graphic Control Extension */
		str = "Graphic Control Extension";
		(void) GetDataBlock(fd, (unsigned char*) buf);
		Gif89.disposal    = (buf[0] >> 2) & 0x7;
		Gif89.inputFlag   = (buf[0] >> 1) & 0x1;
		Gif89.delayTime   = LM_to_uint(buf[1],buf[2]);
		if ((buf[0] & 0x1) != 0)
			Gif89.transparent = buf[3];

		while (GetDataBlock(fd, buf) != 0)
			;
		return False;
	default:
		str = (char *) buf;
		sprintf(str, "UNKNOWN (0x%02x)", label);
		break;
	}

	if (appres.DEBUG)
		fprintf(stderr,"got a '%s' extension\n", str );

	while (GetDataBlock(fd, buf) != 0)
		;

	return False;
}

int	ZeroDataBlock = False;

static int
GetDataBlock(FILE *fd, unsigned char *buf)
{
	unsigned char	count;

	/* error in getting DataBlock size */
	if (! ReadOK(fd,&count,1)) {
		return -1;
	}

	ZeroDataBlock = count == 0;

	/* error in reading DataBlock */
	if ((count != 0) && (! ReadOK(fd, buf, count))) {
		return -1;
	}

	return count;
}
