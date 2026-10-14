/*
 * regex.h - POSIX regular expressions for Windows, on C++ std::regex
 * (regex.cpp).  Only what xfig uses: extended expressions and submatches.
 */
#ifndef XQ_REGEX_H
#define XQ_REGEX_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef long	regoff_t;

typedef struct {
	size_t	re_nsub;	/* number of parenthesized subexpressions */
	void	*impl;
} regex_t;

typedef struct {
	regoff_t	rm_so, rm_eo;
} regmatch_t;

#define REG_EXTENDED	1
#define REG_ICASE	2
#define REG_NOSUB	4
#define REG_NEWLINE	8

#define REG_NOMATCH	1
#define REG_BADPAT	2
#define REG_ESPACE	12

int	regcomp(regex_t *preg, const char *pattern, int cflags);
int	regexec(const regex_t *preg, const char *string, size_t nmatch,
		regmatch_t pmatch[], int eflags);
size_t	regerror(int errcode, const regex_t *preg, char *errbuf,
		size_t errbuf_size);
void	regfree(regex_t *preg);

#ifdef __cplusplus
}
#endif

#endif
