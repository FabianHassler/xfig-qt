/*
 * FIG : Facility for Interactive Generation of figures
 * Copyright (c) 1985-1988 by Supoj Sutanthavibul
 * Parts Copyright (c) 1989-2015 by Brian V. Smith
 * Parts Copyright (c) 1991 by Paul King
 * Parts Copyright (c) 2016-2024 by Thomas Loimer
 *
 * Copyright (c) 1992 by Brian Boyter
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

/* GS bitmap generation added: 13 Nov 1992, by Michael C. Grant
*  (mcgrant@rascals.stanford.edu) adapted from Marc Goldburg's
*  (marcg@rascals.stanford.edu) original idea and code. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "f_picobj.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>		/* time_t */
#include <X11/Intrinsic.h>     /* includes X11/Xlib.h, which includes X11/X.h */

#include "resources.h"		/* TMPDIR */
#include "object.h"
#include "mode.h"
#include "f_readpcx.h"		/* read_pcx() */
#include "f_util.h"		/* file_timestamp() */
#include "u_create.h"		/* create_picture_entry() */
#include "u_spawn.h"
#include "w_file.h"		/* check_cancel() */
#include "w_msgpanel.h"
#include "w_setup.h"		/* PIX_PER_INCH, PIX_PER_CM */
#include "w_util.h"		/* app_flush() */

extern	int	read_gif(F_pic *pic, struct xfig_stream *restrict pic_stream);
extern	int	read_eps(F_pic *pic, struct xfig_stream *restrict pic_stream);
extern	int	read_pdf(F_pic *pic, struct xfig_stream *restrict pic_stream);
extern	int	read_ppm(F_pic *pic, struct xfig_stream *restrict pic_stream);
#ifdef HAVE_TIFF
extern	int	read_tif(F_pic *pic, struct xfig_stream *restrict pic_stream);
#endif
extern	int	read_xbm(F_pic *pic, struct xfig_stream *restrict pic_stream);
#ifdef HAVE_JPEG
extern	int	read_jpg(F_pic *pic, struct xfig_stream *restrict pic_stream);
#endif
#ifdef HAVE_PNG
extern	int	read_png(F_pic *pic, struct xfig_stream *restrict pic_stream);
#endif
#ifdef USE_XPM
extern	int	read_xpm(F_pic *pic, struct xfig_stream *restrict pic_stream);
#endif


static struct _haeders {
	char	*type;
	char	*bytes;
	int	(*readfunc)();
} headers[] = {
	{"GIF",		"GIF",					read_gif},
	{"PCX",		"\012\005\001",				read_pcx},
	{"EPS",		"%!",					read_eps},
	{"PDF",		"%PDF",					read_pdf},
	{"PPM",		"P3",					read_ppm},
	{"PPM",		"P6",					read_ppm},
#ifdef HAVE_TIFF
	{"TIFF",	"II*\000",				read_tif},
	{"TIFF",	"MM\000*",				read_tif},
#endif
	{"XBM",		"#define",				read_xbm},
#ifdef HAVE_JPEG
	{"JPEG",	"\377\330\377\340",			read_jpg},
	{"JPEG",	"\377\330\377\341",			read_jpg},
#endif
#ifdef HAVE_PNG
	{"PNG",		"\211\120\116\107\015\012\032\012",	read_png},
#endif
#ifdef USE_XPM
	{"XPM",		"/* XPM */",				read_xpm},
#endif
};


void
init_stream(struct xfig_stream *restrict xf_stream)
{
	xf_stream->fp = NULL;
	xf_stream->name = xf_stream->name_buf;
	xf_stream->name_on_disk = xf_stream->name_on_disk_buf;
	xf_stream->uncompress = NULL;
	xf_stream->content = xf_stream->content_buf;
	*xf_stream->content = '\0';
}

void
free_stream(struct xfig_stream *restrict xf_stream)
{
	if (xf_stream->content != xf_stream->name_on_disk) {
		if (*xf_stream->content && unlink(xf_stream->content)) {
			int	err = errno;
			file_msg("Cannot remove temporary file %s\nError: %s",
					xf_stream->content, strerror(err));
		}
		if (xf_stream->content != xf_stream->content_buf)
			free(xf_stream->content);
	}
	if (xf_stream->name != xf_stream->name_buf)
		free(xf_stream->name);
	if (xf_stream->name_on_disk != xf_stream->name_on_disk_buf)
		free(xf_stream->name_on_disk);
}

/*
 * Given name, search and return the name of the corresponding file on disk in
 * found. This may be "name", or "name" with a compression suffix appended,
 * e.g., "name.gz". If the file must be uncompressed, return the compression
 * command in a static string pointed to by "uncompress", otherwise let
 * "uncompress" point to the empty string. If the size of the character buffer
 * provided in found is too small to accomodate the string, return a pointer to
 * a malloc()'ed string.
 * Return 0 on success, or FileInvalid if the file is not found.
 * Usage:
 *	char	found_buf[len];
 *	char	*found = found_buf;
 *	file_on_disk(name, &found, sizeof found_buf, &uncompress);
 *	...
 *	if (found != found_buf)
 *		free(found);
 */
static int
file_on_disk(const char *restrict name, char **restrict found, size_t len,
		char **uncompress[restrict])
{
	int		i;
	size_t		name_len;
	char		*suffix;
	struct stat	status;
	static char	*filetypes[][3] = {
		/* sorted by popularity? */
#define FILEONDISK_ADD	5	/* must be max(strlen(filetypes[0][])) + 1 */
		{ ".gz",	"gunzip", "-c" },
		{ ".Z",		"gunzip", "-c" },
		{ ".z",		"gunzip", "-c" },
		{ ".zip",	"unzip", "-p" },
		{ ".bz2",	"bunzip2", "-c" },
		{ ".bz",	"bunzip2", "-c" },
		{ ".xz",	"unxz", "-c" }
	};
	const int	filetypes_len =
				(int)(sizeof filetypes / sizeof(filetypes[1]));

	name_len = strlen(name);
	if (name_len >= len &&
			!(*found = new_string(name_len + FILEONDISK_ADD - 1)))
		return FileInvalid;

	strcpy(*found, name);

	/*
	 * Possibilities, e.g.,
	 *	name		name on disk		uncompress
	 *	img.ppm		img.ppm			""
	 *	img.ppm		img.ppm.gz		gunzip -c
	 *	img.ppm.gz	img.ppm.gz		gunzip -c
	 *	img.ppm.gz	img.ppm			""
	 */
	if (stat(name, &status)) {
		/* File not found. Now try, whether a file with one of
		   the known suffices appended exists. */
		if (len < name_len + FILEONDISK_ADD && (*found =
				new_string(name_len + FILEONDISK_ADD - 1)))
			return FileInvalid;

		suffix = *found + name_len;
		for (i = 0; i < filetypes_len; ++i) {
			strcpy(suffix, filetypes[i][0]);
			if (!stat(*found, &status)) {
				*uncompress = &filetypes[i][1];
				break;
			}
		}

		/* Not found. Check, whether the file has one of the known
		   compression suffices, but the uncompressed file
		   exists on disk. */
		if (i == filetypes_len && (suffix = strrchr(name, '.'))) {
			for (i = 0; i < filetypes_len; ++i) {
				if (!strcmp(suffix, filetypes[i][0])) {
					*(*found + (suffix - name)) = '\0';
					if (!stat(*found, &status)) {
						*uncompress = NULL;
						break;
					} else {
						*found[0] = '\0';
						i = filetypes_len;
					}
				}
			}
		}

		if (i == filetypes_len) {
			/* not found */
			*found[0] = '\0';
			return FileInvalid;
		}
	} else {
		/* File exists. Check, whether the name has one of the known
		   compression suffices. */
		if ((suffix = strrchr(name, '.'))) {
			for (i = 0; i < filetypes_len; ++i) {
				if (!strcmp(suffix, filetypes[i][0])) {
					*uncompress = &filetypes[i][1];
					break;
				}
			}
		} else {
			i = filetypes_len;
		}

		if (i == filetypes_len)
			*uncompress = NULL;
	}

	return 0;
}

/*
 * Compare the picture information in pic with "file". If the file on disk
 * is newer than the picture information, set "reread" to true.
 * If a cached picture bitmap already exists, set "existing" to true.
 * Return FileInvalid, if "file" is not found, otherwise return 0.
 */
static int
get_picture_status(F_pic *pic, struct _pics *pics, bool force,
		bool *reread, bool *existing)
{
	char		found_buf[256];
	char		*found = found_buf;
	char		**uncompress;
	const char	*file = ABSOLUTE_PATH(pics->file);
	time_t		mtime;

	/* get the name of the file on disk */
	if (file_on_disk(file, &found, sizeof found_buf, &uncompress)) {
		if (found != found_buf)
			free(found);
		return FileInvalid;
	}

	/* check the timestamp */
	mtime = file_timestamp(found);
	if (found != found_buf)
		free(found);
	if (mtime < 0) {
		/* oops, doesn't exist? */
		file_msg("Error %s on %s", strerror(errno), file);
		return FileInvalid;
	}
	if (force || mtime > pics->time_stamp) {
		if (appres.DEBUG && mtime > pics->time_stamp)
			fprintf(stderr, "Timestamp changed, reread file %s\n",
					file);
		*reread = true;
		return 0;
	}

	pic->pic_cache = pics;
	pics->refcount++;

	if (appres.DEBUG)
		fprintf(stderr, "Found stored picture %s, count=%d\n", file,
				pics->refcount);

	/* there is a cached bitmap */
	if (pics->bitmap != NULL) {
		*existing = true;
		*reread = false;
		put_msg("Reading Picture object file...found cached picture");
	} else {
		*existing = false;
		*reread = true;
		if (appres.DEBUG)
			fprintf(stderr, "Re-reading file %s\n", file);
	}
	return 0;
}

/*
 * Check through the pictures repository to see if "file" is already there.
 * If so, set the pic->pic_cache pointer to that repository entry and set
 * "existing" to True.
 * If not, read the file via the relevant reader and add to the repository
 * and set "existing" to False.
 * If "force" is true, read the file unconditionally.
 * The string "file" must have been allocated by the caller and not be freed
 * after a call to read_picobj().
 */
void
read_picobj(F_pic *pic, char *file, int color, Boolean force, Boolean *existing)
{
	FILE		*fp;
	int		i;
	char		*abs_path = ABSOLUTE_PATH(file);
	char		buf[16];
	bool		reread;
	struct _pics	*pics, *lastpic;
	struct xfig_stream	pic_stream;

	pic->color = color;
	/* don't touch the flipped flag - caller has already set it */
	pic->pixmap = (Pixmap)0;
	pic->hw_ratio = 0.0;
	pic->pix_rotation = 0;
	pic->pix_width = 0;
	pic->pix_height = 0;
	pic->pix_flipped = 0;

	/* check if user pressed cancel button */
	if (check_cancel())
		return;

	put_msg("Reading Picture object file...");
	app_flush();

	/* look in the repository for this filename */
	lastpic = pictures;
	for (pics = pictures; pics; pics = pics->next) {
		if (!strcmp(ABSOLUTE_PATH(pics->file), abs_path)) {
			/* check, whether picture exists, or must be re-read */
			if (get_picture_status(pic, pics, force, &reread,
						(bool *)existing) ==FileInvalid)
				return;
			if (!reread && *existing) {
				/* must set the h/w ratio here */
				pic->hw_ratio =(float)pic->pic_cache->bit_size.y
					/ pic->pic_cache->bit_size.x;
				free(file);
				return;
			}
			break;
		}
		/* keep pointer to last entry */
		lastpic = pics;
	}

	if (pics == NULL) {
		/* didn't find it in the repository, add it */
		pics = create_picture_entry();
		if (lastpic) {
			/* add to list */
			lastpic->next = pics;
			pics->prev = lastpic;
		} else {
			/* first one */
			pictures = pics;
		}
		pics->file = file;
		pics->refcount = 1;
		pics->bitmap = NULL;
		pics->subtype = T_PIC_NONE;
		pics->numcols = 0;
		pics->size_x = 0;
		pics->size_y = 0;
		pics->bit_size.x = 0;
		pics->bit_size.y = 0;
		if (appres.DEBUG)
			fprintf(stderr, "New picture %s\n", file);
	}
	/* put it in the pic */
	pic->pic_cache = pics;
	pic->pixmap = (Pixmap)0;

	if (appres.DEBUG)
		fprintf(stderr, "Reading file %s\n", file);

	init_stream(&pic_stream);

	/* open the file and read a few bytes of the header to see what it is */
	if ((fp = open_stream(abs_path, &pic_stream)) == NULL) {
		file_msg("Could not open picture file %s", abs_path);
		free_stream(&pic_stream);
		return;
	}
	/* get the modified time and save it */
	pics->time_stamp = file_timestamp(pic_stream.name_on_disk);

	/* read some bytes from the file */
	for (i = 0; i < (int)sizeof buf; ++i) {
		int	c;
		if ((c = getc(fp)) == EOF)
			break;
		buf[i] = (char)c;
	}

	/* now find which header it is */
	for (i = 0; i < (int)(sizeof headers / sizeof(headers[0])); ++i)
		if (!memcmp(buf, headers[i].bytes, strlen(headers[i].bytes)))
			break;

	/* not found */
	if (i == (int)(sizeof headers / sizeof(headers[0]))) {
		file_msg("%s: Unknown image format", abs_path);
		put_msg("Reading Picture object file...Failed");
		app_flush();
		close_stream(&pic_stream);
		free_stream(&pic_stream);
		return;
	}

	/* readfunc() expect an open file stream, positioned not at the
	   start of the stream. The stream remains open after returning. */
	if (headers[i].readfunc(pic, &pic_stream) != PicSuccess) {
		file_msg("Errors occurred when reading %s file %s",
						headers[i].type, abs_path);
	} else {
		put_msg("Reading Picture object file...Done");
	}

	close_stream(&pic_stream);
	free_stream(&pic_stream);

	if (pics->refcount > 1)
		free(file);
}

/*
 * Return a file stream, either to a pipe or to a regular file.
 * If xf_stream->uncompress == NULL it is a regular file, otherwise a pipe.
 */
FILE *
open_stream(char *restrict name, struct xfig_stream *restrict xf_stream)
{
	size_t	len;

	if (xf_stream->name != name) {
		/* strcpy (xf_stream->name, name) */
		len = strlen(name);
		if (len >= sizeof xf_stream->name_buf) {
			if (!(xf_stream->name = new_string(len)))
				return NULL;
		}
		memcpy(xf_stream->name, name, len + 1);
	}

	if (file_on_disk(name, &xf_stream->name_on_disk,
				sizeof xf_stream->name_on_disk_buf,
				&xf_stream->uncompress)) {
		free_stream(xf_stream);
		return NULL;
	}

	if (xf_stream->uncompress) {
		/* a compressed file */

		int	fd;
		char	*args[4];

		args[0] = xf_stream->uncompress[0];
		args[1] = xf_stream->uncompress[1];
		args[2] = xf_stream->name_on_disk;
		args[3] = NULL;

		if ((fd = spawn_popen(args, NULL, "r")) == -1) {
			xf_stream->fp = NULL;
			return NULL;
		}
		if (!(xf_stream->fp = fdopen(fd, "r")))
			file_msg("Cannot read uncompressed content of %s: %s",
					xf_stream->name_on_disk,
					strerror(errno));

	} else {
		/* uncompressed file */

		if (!(xf_stream->fp = fopen(xf_stream->name_on_disk, "rb")))
			file_msg("Unable to open %s: %s",
					xf_stream->name_on_disk,
					strerror(errno));
	}

	return xf_stream->fp;
}

/*
 * Close a file stream opened by open_file(). Do not free xf_stream.
 */
int
close_stream(struct xfig_stream *restrict xf_stream)
{
	if (xf_stream->fp == NULL)
		return -1;

	if (!xf_stream->uncompress) {
		/* a regular file */
		return fclose(xf_stream->fp);
	} else {
		/* a pipe */
		return spawn_pclose(fileno(xf_stream->fp));
	}
}

FILE *
rewind_stream(struct xfig_stream *restrict xf_stream)
{
	if (xf_stream->fp == NULL)
		return NULL;

	if (!xf_stream->uncompress) {
		/* a regular file */
		/* The file might be read with the raw system calls via its file
		   descriptor, or with fread. Positioning of the stream and the
		   file position is independent of each other. With rewind(),
		   the stream might be at the beginning, but open() commences
		   somwhere in the file. Likewise, lseek() does not necessarily
		   set the stream to the beginning of the file.
		   Do both. */
		rewind(xf_stream->fp);
		lseek(fileno(xf_stream->fp), 0, SEEK_SET);
		return xf_stream->fp;
	} else  {
		/* a pipe */
		(void)close_stream(xf_stream);
		/* if, in the meantime, e.g., the file on disk
		   was uncompressed, change the filetype. */
		return open_stream(xf_stream->name, xf_stream);
	}
}

/*
 * Have xf_stream->content either point to a regular file containing the
 * uncompressed content of xf_stream->name, or to xf_stream->name_on_disk, if
 * name_on_disk is not compressed.
 * Use after a call to open_stream():
 *	struct xfig_stream	xf_stream;
 *	open_stream(name, &xf_stream);
 *	close_stream(&xf_stream);
 *	uncompressed_content(&xf_stream)
 *	do_something(xf_stream->content);
 *	free_stream(&xf_stream);
 */
int
uncompressed_content(struct xfig_stream *restrict xf_stream)
{
	int		ret = -1;
	int		fd;
	int		len;
	char		*args[4];
	char *const	content_fmt = "%s/xfigXXXXXX";

	if (!xf_stream->uncompress) {
		xf_stream->content = xf_stream->name_on_disk;
		return 0;
	}

	/* uncompress to a temporary file */

	len = snprintf(xf_stream->content, sizeof xf_stream->content_buf,
			content_fmt, TMPDIR);
	if (len >= (int)(sizeof xf_stream->content_buf)) {
		if (!(xf_stream->content = new_string(len)))
			return ret;
		len = sprintf(xf_stream->content, content_fmt, TMPDIR);
	}
	if (len < 0) {
		len = errno;
		file_msg("Unable to write temporary file path to string.");
		file_msg("Error: %s", strerror(len));
		return ret;
	}

	if ((fd = mkstemp(xf_stream->content)) == -1) {
		file_msg("Could not open temporary file %s: %s",
				xf_stream->content, strerror(errno));
		return ret;
	}

	args[0] = xf_stream->uncompress[0];
	args[1] = xf_stream->uncompress[1];
	args[2] = xf_stream->name_on_disk;
	args[3] = NULL;

	/* spawn_usefd() gives sufficient error information */
	ret = spawn_usefd(args, NULL, -1, fd);
	if (close(fd))
		file_msg("Error closing temporary file %s: %s",
				xf_stream->content, strerror(errno));
	return ret;
}

/*
 * Compute the image dimension (size_x, size_y) in Fig-units from the number of
 * pixels and the resolution in x- and y-direction, pixels_x, pixels_y, and
 * res_x and res_y, respectively.
 * The resolution is given in pixels / cm or pixels / inch, depending on
 * whether 'c' or 'i' is passed to unit. For any other character passed to unit,
 * a resolution of 72 pixels per inch is assumed.
 */
void
image_size(int *size_x, int *size_y, int pixels_x, int pixels_y,
		char unit, float res_x, float res_y)
{
	/* exclude negative and absurdly small values */
	if (res_x < 1. || res_y < 1.) {
		res_x = 72.0f;
		res_y = 72.0f;
		unit = 'i';
	}

	if (unit == 'i') {		/* pixel / inch */
		if (appres.INCHES) {
			*size_x = pixels_x * PIX_PER_INCH / res_x + 0.5;
			*size_y = pixels_y * PIX_PER_INCH / res_y + 0.5;
		} else {
			*size_x = pixels_x * PIX_PER_CM * 2.54 / res_x + 0.5;
			*size_y = pixels_y * PIX_PER_CM * 2.54 / res_y + 0.5;
		}
	} else if (unit == 'c') {	/* pixel / cm */
		if (appres.INCHES) {
			*size_x = pixels_x * PIX_PER_INCH / (res_x * 2.54) +0.5;
			*size_y = pixels_y * PIX_PER_INCH / (res_y * 2.54) +0.5;
		} else {
			*size_x = pixels_x * PIX_PER_CM / res_x + 0.5;
			*size_y = pixels_y * PIX_PER_CM / res_y + 0.5;
		}
	} else {			/* unknown, default to 72 ppi */
		*size_x = pixels_x * PIC_FACTOR + 0.5;
		*size_y = pixels_y * PIC_FACTOR * res_x / res_y + 0.5;
	}
}

/*
 * File paths to images are displayed in the picture editing panel, widget
 * pic_name_panel, and written to the .fig file as paths relative to
 * cur_file_dir. If the user enters an absolute path, the absolute path is
 * written to the .fig file. However, the file entry in the struct _pics
 * pic_cache always contains an absolute path, because the absolute path is used
 * to identify images and to not read the same image twice. Also, relative paths
 * in picture objects would all have to be updated, if cur_file_dir changes.
 *
 * Prepend the absolute path in pic_cache->file with a second slash, if it
 * should be displayed or written as absolute path. Prepend it with a tilde, if
 * the relative path to the home directory should be used.
 *
 * Example, if cur_file_dir="/path/output/dir":
 *	internal (pic_cache->file)	external (pic_name_panel, saved)
 *	/path/picture_dir/tiger.png	../../picture_dir/tiger.png
 *	//path/picture_dir/tiger.png	/path/picture_dir/tiger.png
 *	~/home/user/pics/tiger.png	~/pics/tiger.png
 */

/*
 * Input:
 *    rel_or_abs_path	A path relative to cur_file_dir (not starting with a
 *			slash), relative to home (starting with ~), or
 *			an absolute path
 * Return value:
 *    A malloc'd string containing the internal representation of
 *    rel_or_abs_path, or a guess of the path if the file cannot be found.
 *    NULL if rel_or_abs_path is empty.
 *    The internal representation is the absolute path for a path relative to
 *    cur_file_dir, the absolute path prepended with ~ for a path relative to
 *    home dir, or with a second slash prepended for an absolute path.
 *    A path relative to cur_file_dir is internally represented by the absolute
 *    path
 */
char *
internal_path(const char *restrict rel_or_abs_path)
{
	size_t	rel_abs_len;
	size_t	guessed_size;
	size_t	offset;
	char	first_char;
	char	guessed_abs_path_buf[1024];
	char	*guessed_abs_path = guessed_abs_path_buf;
	char	*abs_path;
	char	*result;

	if (*rel_or_abs_path == '\0')
		return NULL;

	rel_abs_len = strlen(rel_or_abs_path);

	/* construct the tentative absolute path,
	   possibly with ~ or / appended	*/
	if (*rel_or_abs_path == '/') {
		guessed_size = rel_abs_len + 2;
		if (guessed_size > sizeof guessed_abs_path_buf &&
				!(guessed_abs_path =new_string(guessed_size-1)))
			return NULL;
		memcpy(guessed_abs_path + 1, rel_or_abs_path, rel_abs_len + 1);
		guessed_abs_path[0] = first_char = '/';
		offset = 1;
	} else if (*rel_or_abs_path == '~') {
		char	*home = getenv("HOME");
		size_t	home_len = strlen(home);

		/* correct ~some/path to ~/some/path: len + '/' + '\0' */
		guessed_size = rel_abs_len + home_len + 2;
		if (guessed_size > sizeof guessed_abs_path_buf &&
				!(guessed_abs_path =new_string(guessed_size-1)))
			return NULL;
		memcpy(guessed_abs_path + 1, home, home_len);
		if (rel_or_abs_path[1] != '/')
			guessed_abs_path[1 + home_len++] = '/';
		memcpy(guessed_abs_path + 1 + home_len, rel_or_abs_path + 1,
							rel_abs_len);
		guessed_abs_path[0] = first_char = '~';
		offset = 1;
	} else {
		size_t	dir_len = strlen(cur_file_dir);

		guessed_size = rel_abs_len + dir_len + 2;
		if (guessed_size > sizeof guessed_abs_path_buf &&
				!(guessed_abs_path =new_string(guessed_size-1)))
			return NULL;
		memcpy(guessed_abs_path, cur_file_dir, dir_len);
		guessed_abs_path[dir_len] = '/';
		memcpy(guessed_abs_path + dir_len + 1, rel_or_abs_path,
							rel_abs_len + 1);
		offset = 0;
	}

	/* write the real absolute path to abs_path */
	abs_path = realpath(guessed_abs_path + offset, NULL);

	/* if realpath failed, return the guessed path */
	if (!abs_path) {
		if (guessed_abs_path != guessed_abs_path_buf) {
			result = guessed_abs_path;
		} else {
			if (!(result = new_string(guessed_size - 1)))
				return NULL;
			memcpy(result, guessed_abs_path, guessed_size);
		}
		return result;
	}
	if (guessed_abs_path != guessed_abs_path_buf)
		free(guessed_abs_path);

	/* finally, return the result */
	if (offset) {
		size_t	len = strlen(abs_path);
		if (!(result = new_string(len + offset)))
			return NULL;
		memcpy(result + offset, abs_path, len + 1);
		result[0] = first_char;
		free(abs_path);
	} else {
		result = abs_path;
	}

	return result;
}

/*
 * Return the path of target_file relative to source_dir. Both target_file and
 * source_dir must be absolute path names returned by realpath().
 * Write the result into size-sized buffer relname.
 * Return the number of characters written.
 * A return value of size or more means that the buffer pointed to by relname
 * was too small.
 */
static int
xf_relpath(const char *restrict source_dir, const char *restrict target_file,
	char *relname, size_t size)
{
	int	i = 0;
	size_t	required_size;
	size_t	one = strlen("../");

	/* the index of the first char not common to both s and t */
	while (*source_dir && *target_file && *source_dir == *target_file) {
		++source_dir;
		++target_file;
		++i;
	}
	/* source: /a/this, target: /a/that - rewind to the rightmost slash */
	if (*source_dir != '/' && *source_dir != '\0') {
		while (i > 0 && *--source_dir != '/') {
			--target_file;
			--i;
		}
		++source_dir;
	}

	/*
	 * If the target_file is located below source_dir, the first char in
	 * target_file is a slash, and source_dir points to '\0'. Otherwise, one
	 * more than '/' present in source_dir times '../' must be written to
	 * relname, and target_file appended.
	 * /a/dir, /a/myfile	 -> dir, myfile: ../myfile.
	 * /a/dir, /a/dir/myfile -> "", /myfile: myfile.
	 * /a/this, /a/that	 -> this, that: ../that
	 */

	if (*source_dir) {
		i = 1;
		/* the first char cannot be '/' */
		while ((source_dir = strchr(++source_dir, '/')))
			++i;
	} else { /* *source_dir == '\0', *target_file == '/' */
		i = 0;
		++target_file;
	}
	required_size = i * one + strlen(target_file);
	if (required_size >= size) {
		*relname = '\0';
	} else {
		for (; i > 0; --i) {
			strcpy(relname, "../");
			relname += one;
		}
		strcpy(relname, target_file);
	}
	return required_size;
}

/*
 * Return the path of target_file relative to source_dir. The path of source_dir
 * is canonicalized with realpath(), target_file must be an absolute path name
 * returned by realpath(). Write the result into size-sized buffer relname.
 * Return the number of characters written, or -1 if the path of source_dir
 * cannot be found. A return value of size or more means that the buffer pointed
 * to by relname was too small.
 */
static int
relative_path(const char *restrict source_dir, const char *restrict target_file,
		char *relname, size_t size)
{
	int	status;
	char	*result;

	if ((result = realpath(source_dir, NULL))) {
		status = xf_relpath(result, target_file, relname, size);
		free(result);
	} else {
		/* if source_dir cannot be found, return the absolute path */
		*relname = '\0';
		status = -1;
	}
	return status;
}

/*
 * Inputs:
 *	internal	an internal path //abs/path, ~/home/user/rel, /rel/path
 *	size		the size of the buffer passed in rel_or_abs_path
 * Output:
 *	rel_or_abs_path	  external representation: /abs/path, ~/rel, ../path
 *			  or an absolute path, if cur_file_dir cannot be found
 * Return value:
 *	The number of characters written or, if this number is larger or equal
 *	to size, the number of characters that would have been written to
 *	rel_or_abs_path.
 *	-1 on error, unexpected internal path
 */
static int
xf_external_path(char *rel_or_abs_path, size_t size, char *internal)
{
	size_t	len;

	if (internal[0] == '/') {
		size_t	offset;
		if (internal[1] != '/') {
			/* relative path: /abs/dir/file */
			int	status = relative_path(cur_file_dir, internal,
					rel_or_abs_path, size);
			if (status >= 0)
				return status;
			/* fall through if the relative path cannot be
			 * constructed, and return an absolute path.	*/
			len = strlen(internal);
			offset = 0;
		} else {	/* internal[1] == '/' */
			/* absolute path: //abs/dir/file */
			len = strlen(internal) - 1;
			offset = 1;
		}		/* internal[1] == '/' || status < 0 */
		/* absolute path, or fall through */
		if (len + 1 > size) {
			*rel_or_abs_path ='\0';
			return (int)len;
		}
		memcpy(rel_or_abs_path, internal + offset, len + 1);
		return (int)len;

	} else if (internal[0] == '~') {
		char	*home = getenv("HOME");
		size_t	home_len = strlen(home);

		if (!strncmp(home, internal + 1, home_len) &&
				internal[1 + home_len] == '/') {
			len = strlen(internal) - home_len;
			if (len + 1 > size) {
				*rel_or_abs_path ='\0';
				return (int)len;
			}
			rel_or_abs_path[0] = '~';
			memcpy(rel_or_abs_path + 1, internal + home_len + 1,
							len);
		} else {
			/* return the absolute path, if $HOME is not found */
			len = strlen(internal) - 1;
			if (len + 1 > size) {
				*rel_or_abs_path ='\0';
				return (int)len;
			}
			memcpy(rel_or_abs_path, internal + 1, len + 1);
		}
		return (int)len;

	} else {
		file_msg("Error in xf_external_path(): "
				"Unexpected internal path %s", internal);
		*rel_or_abs_path ='\0';
		return -1;
	}
}

/*
 * Inputs:
 *    internal	an internal representation of a picture file path,
 *		see the comments above internal_path
 *    size	the size of the buffer passed in rel_or_abs_path
 * Output:
 *    rel_or_abs_path	the external representation of internal. if
 *			cur_file_dir cannot be found, return an absolute path
 * Return value:
 *    The number of chars written, or to be written.
 *    On error, return a value smaller than zero.
 * If the buffer passed in rel_or_abs_path is of insufficient size, return a
 * newly allocated string. In that case, the value returned by external_path is
 * equal or larger than size.
 *
 * Usage example:
 *	char	name_buf[SIZE];
 *	char	*name = name_buf;
 *	external_path(&name, sizeof name, internal);
 *	if (name != name_buf)
 *		free(name);
 */
int
external_path(char **rel_or_abs_path, size_t size, char *internal)
{
	int	len = xf_external_path(*rel_or_abs_path, size, internal);

	if (len < (int)size)	/* includes len < 0 */
		return len;

	if (!(*rel_or_abs_path = new_string(len)))
		return -1;

	return xf_external_path(*rel_or_abs_path, size, internal);
}
