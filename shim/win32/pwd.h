/*
 * pwd.h - for Windows: there is no user database, so "~user/" paths are not
 * expanded ("~/" is, from HOME).
 */
#ifndef XQ_PWD_H
#define XQ_PWD_H

#include <sys/types.h>

struct passwd {
	char	*pw_name;
	char	*pw_dir;
};

struct passwd	*getpwnam(const char *name);
struct passwd	*getpwuid(int uid);

#endif
