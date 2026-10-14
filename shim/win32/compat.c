/*
 * compat.c - the POSIX functions of xq_win32.h, for Windows.
 */
#include <errno.h>
#include <io.h>
#include <stdlib.h>
#include <string.h>

#include "xq_win32.h"
#include "pwd.h"
#include "langinfo.h"
#include "sys/wait.h"

#undef access

int
xq_access(const char *path, int mode)
{
	/* 1 is X_OK, which Windows' _access() rejects */
	return _access(path, mode & ~1);
}

pid_t
fork(void)
{
	errno = ENOSYS;
	return -1;
}

int
kill(pid_t pid, int sig)
{
	(void)pid;
	(void)sig;
	errno = ENOSYS;
	return -1;
}

pid_t
waitpid(pid_t pid, int *status, int options)
{
	(void)pid;
	(void)status;
	(void)options;
	errno = ECHILD;
	return -1;
}

uid_t getuid(void) { return 0; }
uid_t geteuid(void) { return 0; }

char *
strndup(const char *s, size_t n)
{
	size_t	len = strnlen(s, n);
	char	*d = malloc(len + 1);
	if (d) {
		memcpy(d, s, len);
		d[len] = '\0';
	}
	return d;
}

char *
realpath(const char *path, char *resolved)
{
	char	*r = _fullpath(resolved, path, resolved ? _MAX_PATH : 0);
	if (r && _access(r, 0) != 0) {
		if (!resolved)
			free(r);
		errno = ENOENT;
		return NULL;
	}
	return r;
}

int
setenv(const char *name, const char *value, int overwrite)
{
	if (!overwrite && getenv(name))
		return 0;
	return _putenv_s(name, value) == 0 ? 0 : -1;
}

int
unsetenv(const char *name)
{
	return _putenv_s(name, "") == 0 ? 0 : -1;
}

struct passwd *
getpwnam(const char *name)
{
	(void)name;
	return NULL;
}

struct passwd *
getpwuid(int uid)
{
	(void)uid;
	return NULL;
}

char *
nl_langinfo(nl_item item)
{
	return item == CODESET ? "UTF-8" : "";
}
