/*
 * rewind.c - rewind() for xfig on macOS.
 *
 * To rewind a picture file, xfig calls rewind(fp) and then lseek(fd, 0)
 * (rewind_stream() in f_picobj.c).  macOS's stdio keeps its buffer over
 * rewind(); the next read returns the buffered start of the file and then
 * continues where the descriptor is, again at its start: JPEG pictures
 * failed with "two SOI markers".  Discard the buffer first, as glibc does.
 */
#include <stdio.h>

#undef rewind

void
xq_rewind(FILE *fp)
{
	fpurge(fp);
	rewind(fp);
}
