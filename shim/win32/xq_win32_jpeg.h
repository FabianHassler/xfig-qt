/*
 * xq_win32_jpeg.h - for f_readjpg.c on Windows: MSYS2's jpeglib.h includes
 * windows.h, whose RECT clashes with xfig's.  Include it first, with its
 * RECT renamed; jpeglib.h's include then does nothing.
 */
#define RECT	XQ_WIN32_RECT
#include <windows.h>
#undef RECT
