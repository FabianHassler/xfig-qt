/*
 * langinfo.h - for Windows: Qt hands xfig its text as UTF-8.
 */
#ifndef XQ_LANGINFO_H
#define XQ_LANGINFO_H

typedef int	nl_item;
#define CODESET	0

char	*nl_langinfo(nl_item item);

#endif
