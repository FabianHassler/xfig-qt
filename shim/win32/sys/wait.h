/*
 * sys/wait.h - for Windows; processes are waited for in shim/u_spawn_win.c.
 */
#ifndef XQ_SYS_WAIT_H
#define XQ_SYS_WAIT_H

#include <sys/types.h>

#define WNOHANG		1
#define WIFEXITED(s)	1
#define WEXITSTATUS(s)	(s)
#define WIFSIGNALED(s)	0
#define WTERMSIG(s)	0

pid_t	waitpid(pid_t pid, int *status, int options);

#endif
