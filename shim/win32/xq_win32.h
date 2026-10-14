/*
 * xq_win32.h - the POSIX parts xfig uses that Windows (MinGW-w64) lacks.
 *
 * On Windows, CMakeLists.txt includes this before every C file, and puts
 * shim/win32 on the include path for the stand-in headers there (pwd.h,
 * sys/wait.h, langinfo.h).  Processes are spawned by shim/u_spawn_win.c.
 * What cannot work on Windows (fork(), signals to other processes) fails
 * with ENOSYS, and xfig reports it.
 */
#ifndef XQ_WIN32_H
#define XQ_WIN32_H

/* the system headers first: the macros below rename some of their
   functions in xfig's code only */
#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SIGHUP
#define SIGHUP		1
#endif
#ifndef SIGQUIT
#define SIGQUIT		3
#endif
#ifndef SIGKILL
#define SIGKILL		9
#endif
#ifndef SIGUSR1
#define SIGUSR1		10
#endif
#ifndef SIGPIPE
#define SIGPIPE		13
#endif

typedef char	*caddr_t;

/* a macro: some MinGW versions have these types */
#define uid_t	int
#define gid_t	int

/* no symbolic links */
#define lstat	stat

/* access(): Windows has no execute permission bit; X_OK fails there */
#define access	xq_access
int	xq_access(const char *path, int mode);

/* xfig declares getcwd(char *, size_t), Windows getcwd(char *, int) */
#define getcwd	xq_getcwd
char	*xq_getcwd(char *buf, size_t size);

/* xfig sets O_APPEND on its print batch file, written in order anyway */
#ifndef F_GETFL
#define F_GETFL	3
#define F_SETFL	4
#endif
int	fcntl(int fd, int cmd, ...);
#define fsync	_commit

double	drand48(void);
void	srand48(long seed);
long	random(void);
void	srandom(unsigned int seed);

pid_t	fork(void);
int	kill(pid_t pid, int sig);
uid_t	getuid(void);
uid_t	geteuid(void);
char	*strndup(const char *s, size_t n);
char	*realpath(const char *path, char *resolved);
int	setenv(const char *name, const char *value, int overwrite);
int	unsetenv(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* XQ_WIN32_H */
