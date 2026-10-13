/*
 * FIG : Facility for Interactive Generation of figures
 * Copyright (c) 1985-1988 by Supoj Sutanthavibul
 * Parts Copyright (c) 1989-2015 by Brian V. Smith
 * Parts Copyright (c) 1991 by Paul King
 * Parts Copyright (c) 2016-2023 by Thomas Loimer
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
 * Text conversion between local charset and utf-8.
 * See also fig2dev/fig2dev/dev/textconvert.c
 */

#ifdef HAVE_CONFIG_H
#include "config.h"		/* restrict */
#endif
#include <u_convert.h>

#include <stdio.h>
#include <string.h>

#ifdef HAVE_ICONV
#include <errno.h>
#include <iconv.h>
#include <stdlib.h>
#include "w_msgpanel.h"
#endif

#ifdef HAVE_NL_LANGINFO
#include <langinfo.h>
#else
#include <locale.h>
#endif


#ifdef HAVE_ICONV
static int	need_conversion = -1;	/* -1 - not set, 0 - not needed,
					    1 - needed */
static iconv_t	cd[2];			/* 0: local to utf8,
					   1: utf8 to local */

/*
 * Write the name of the local charset into charset[size].
 * Return 0 on success, -1 on failure.
 */
static int
get_local_charset(char *restrict charset, size_t size)
{
	char	*c;
	size_t	size_c;

#ifdef HAVE_NL_LANGINFO
	c = nl_langinfo(CODESET);
	size_c = strlen(c) + 1;
	if (size_c > size) {
		file_msg("Cannot deal with charset names longer than %zd bytes:"
					" %s", size, c);
		return -1;
	}
	memcpy(charset, c, size_c);
#else
	/* try to read the codeset from the string after the full stop */
	if (!(c = strchr(setlocale(LC_CTYPE, NULL), '.')))
		return -1;
	++c;
	size_c = strlen(c) + 1;
	if (size_c > size) {
		file_msg("Cannot deal with charset names longer than %zd bytes:"
					" %s", size, c);
		return -1;
	}

	/* convert, e.g., iso88592 to ISO-8859-2 and utf8 to UTF-8 */
	if (!strcmp(c, "utf8")) {
		if (size < sizeof("UTF-8")) {
			file_msg("Need a buffer larger than %zd to store UTF-8 "
					"as the codeset name", size);
			return -1;
		}
		strcpy(charset, "UTF-8");
	} else if (!strcmp(c,"iso8859") && strlen(c+=strlen("iso8859")) < 3) {
		if (size < sizeof("ISO-8859-10")) {
			file_msg("Need a buffer larger than %zd to store an "
					"ISO-8859-* charset name", size);
			return -1;
		}
		strcpy(charset, "ISO-8859-");
		strcat(charset, c);
	} else {
		memcpy(charset, c, size_c);
	}
#endif /* HAVE_NL_LANGINFO */
	return 0;
}

/*
 * Assign need_conversion and the conversion specifiers cd[0] and cd[1].
 */
static void
init_conversion(void)
{
	char	charset[32];

	if (need_conversion != -1)
		return;

	if (get_local_charset(charset, sizeof charset)) {
		need_conversion = 0;
		return;
	}

	if (!strcmp(charset, "UTF-8")) {
		need_conversion = 0;
		return;
	}

	if ((cd[0] = iconv_open("UTF-8", charset)) == (iconv_t)-1) {
		file_msg("Unable to convert from %s to UTF-8 character set",
				charset);
		need_conversion = 0;
	}
	if ((cd[1] = iconv_open(charset, "UTF-8")) == (iconv_t)-1) {
		file_msg("Unable to convert from UTF-8 to %s character set",
				charset);
		need_conversion = 0;
	}
	need_conversion = 1;
}

static void
convert(char **restrict out, char *restrict in, iconv_t	lcd)
{
	size_t	stat;
	size_t	inlen;
	size_t	out_remain;
	size_t	out_size;
	size_t	in_remain;
	char	*inpos;
	char	*outpos;
#define Err_mem		"Running out of memory"

	if (need_conversion == 0 || in[0] == '\0') {
		 *out = strdup(in);
		 return;
	}

	/* This precludes encodings with characters containing '\0'. */
	inlen = strlen(in);

	/* allocate a large enough output buffer */
	out_size = inlen > 8 ? 2 * inlen : 16;
	if (!(*out = malloc(out_size))) {
		put_msg(Err_mem);
		return;
	}
	out_remain = out_size;
	in_remain = inlen + 1;		/* also convert the final '\0' */
	inpos = in;
	outpos = *out;
	errno = 0;
	while ((stat = iconv(lcd, &inpos, &in_remain, &outpos, &out_remain))
			&& in_remain != 0 && errno == E2BIG) {
		/* output size too small */
		size_t		to_outpos = (size_t)(outpos - *out);
		size_t		mult = 2;
		size_t		old_size = out_size;
		/* compute the expansion factor from in to out */
		if (inpos - in > 0) {
			mult = to_outpos / (size_t)(inpos - in) + 1;
			if (mult < 2)
				mult = 2;
		}
		if (!(*out = realloc(*out, out_size *= mult))) {
			put_msg(Err_mem);
			return;
		}
		/* continue the conversion from where it stopped */
		outpos = *out + to_outpos;
		out_remain += out_size - old_size;
		errno = 0;
	}

	if (stat == (size_t)-1) {
		if (errno == EINVAL)
			file_msg("Conversion error, input string '%s' "
					"prematurely terminated.", in);
		else
			file_msg("Error converting string %s", in);
	}

	/*
	 * Reset the output to its initial conversion state.
	 */
	while (iconv(lcd, NULL, &in_remain, &outpos, &out_remain) == (size_t)-1
			&& errno == E2BIG) {
		const size_t	enlarge = 8;
		size_t		to_outpos = (size_t)(outpos - *out);
		if (!(*out = realloc(*out, out_size += enlarge))) {
			put_msg(Err_mem);
			return;
		}
		outpos = *out + to_outpos;
		out_remain += enlarge;
	}

	*out = realloc(*out, outpos - *out);
}

#endif /* HAVE_ICONV */

/*
 * Return a string converted from local charset to utf8.
 */
char *
conv_utf8strdup(const char *src /* locale charset */)
{
#ifdef HAVE_ICONV
	char	*dest;
#endif

	if (src[0] == '\0')
		return NULL;

#ifdef HAVE_ICONV
	init_conversion();
	convert(&dest, (char *)src, cd[0]);
	return dest;
#else
	return strdup(src);
#endif
}

/*
 * Return a string converted from utf8 to local charset.
 */
char *
conv_strutf8dup(const char *src /* utf8-encoded */)
{
#ifdef HAVE_ICONV
	char	*dest;

	init_conversion();
	convert(&dest, (char *)src, cd[1]);
	return dest;
#else
	return strdup(src);
#endif
}
