/*
 * u_spawn_win.c - xfig's u_spawn.c for Windows: start fig2dev, ghostscript
 * and the other helper programs with their stdin, stdout and stderr
 * connected to file descriptors.
 *
 * Windows has no fork() or posix_spawn().  The C runtime's _spawnvpe()
 * gives the new process the parent's descriptors 0, 1 and 2, so these are
 * pointed at the wanted descriptors for the call, then restored.  Pipes
 * are made non-inheritable, so that the process sees only its own ends.
 * As in u_spawn.c, stderr goes to a pipe and is shown in the message
 * panel, and a reading pipe that is closed early ends the process.
 */
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "u_spawn.h"
#include "w_msgpanel.h"

#define PIPE_SIZE	65536

static struct process_info {
	int			fd;
	int			fderr;
	intptr_t		handle;
	char			type;
	struct process_info	*next;
} *processes = NULL;

static void
add_info(int fd, int fderr, intptr_t handle, char type)
{
	struct process_info *info = malloc(sizeof *info);
	if (!info)
		return;
	info->fd = fd;
	info->fderr = fderr;
	info->handle = handle;
	info->type = type;
	info->next = processes;
	processes = info;
}

static int
retrieve_info(int fd, int *fderr, intptr_t *handle, char *type)
{
	struct process_info **p = &processes;
	while (*p && (*p)->fd != fd)
		p = &(*p)->next;
	if (!*p)
		return -1;
	struct process_info *info = *p;
	*fderr = info->fderr;
	*handle = info->handle;
	*type = info->type;
	*p = info->next;
	free(info);
	return 0;
}

static char *
full_command(char *const argv[])
{
	static char	buf[256];
	size_t		pos = 0;

	buf[0] = '\0';
	for (char *const *c = argv; *c && pos < sizeof buf - 1; ++c)
		pos += snprintf(buf + pos, sizeof buf - pos, "%s%s",
				pos ? " " : "", *c);
	return buf;
}

/* Windows passes one command line; quote an argument for the C runtime
   of the started program (backslashes are literal except before ") */
static char *
quote_arg(const char *a)
{
	size_t	len = strlen(a);
	char	*q = malloc(2 * len + 3), *p = q;

	if (!q)
		return NULL;
	if (len && !strpbrk(a, " \t\n\v\"")) {
		memcpy(q, a, len + 1);
		return q;
	}
	*p++ = '"';
	for (const char *s = a; ; ++s) {
		size_t nbs = 0;
		while (*s == '\\') {
			++s;
			++nbs;
		}
		if (*s == '\0') {
			/* double the backslashes before the closing quote */
			for (size_t i = 0; i < 2 * nbs; ++i)
				*p++ = '\\';
			break;
		}
		if (*s == '"') {
			for (size_t i = 0; i < 2 * nbs + 1; ++i)
				*p++ = '\\';
		} else {
			for (size_t i = 0; i < nbs; ++i)
				*p++ = '\\';
		}
		*p++ = *s;
	}
	*p++ = '"';
	*p = '\0';
	return q;
}

/* a non-inheritable pipe: pd[0] reads, pd[1] writes */
static int
make_pipe(int pd[2])
{
	return _pipe(pd, PIPE_SIZE, _O_BINARY | _O_NOINHERIT);
}

/*
 * Start argv[0] with stdin, stdout, stderr on fd[0], fd[1], fd[2] (-1: keep
 * the parent's).  Return the process handle, or -1 with errno set.
 */
static intptr_t
spawn_process(char *const argv[], char *const envp[], int fd[3])
{
	int		saved[3] = {-1, -1, -1};
	intptr_t	h;
	int		i, n, err;
	char		**qargv;

	for (n = 0; argv[n]; ++n)
		;
	if (!(qargv = calloc(n + 1, sizeof *qargv)))
		return -1;
	for (i = 0; i < n; ++i)
		if (!(qargv[i] = quote_arg(argv[i]))) {
			n = i;
			h = -1;
			goto out;
		}

	fflush(NULL);
	for (i = 0; i < 3; ++i) {
		if (fd[i] < 0)
			continue;
		saved[i] = _dup(i);
		_dup2(fd[i], i);
	}
	h = envp ? _spawnvpe(_P_NOWAIT, argv[0], (const char *const *)qargv,
				(const char *const *)envp)
		: _spawnvp(_P_NOWAIT, argv[0], (const char *const *)qargv);
	err = errno;
	for (i = 0; i < 3; ++i) {
		if (saved[i] < 0)
			continue;
		_dup2(saved[i], i);
		_close(saved[i]);
	}
	errno = err;
out:
	for (i = 0; i < n; ++i)
		free(qargv[i]);
	free(qargv);
	return h;
}

static int
wait_process(intptr_t handle, int terminated)
{
	int	status;

	if (_cwait(&status, handle, 0) == -1) {
		file_msg("Error waiting for spawned process: %s",
				strerror(errno));
		return -1;
	}
	return terminated ? 0 : status;
}

/* read what the process wrote to stderr, show the first 255 bytes */
static void
report_stderr(int fderr)
{
	char	buf[256];
	int	n, num = 0;

	while (num < (int)sizeof buf - 1 &&
			(n = _read(fderr, buf + num, sizeof buf - 1 - num)) > 0)
		num += n;
	if (num > 0) {
		buf[num] = '\0';
		file_msg("Error message from spawned process: %s", buf);
	}
	/* read the rest, so that the process can finish */
	while (_read(fderr, buf, sizeof buf) > 0)
		;
	_close(fderr);
}

static int
open_process(char *const argv[], char *const envp[], int fd[2],
		intptr_t *handle, int *fderr)
{
	int	pd[2];
	int	ffd[3];

	if (make_pipe(pd)) {
		file_msg("Spawning %s, cannot create pipe: %s",
				full_command(argv), strerror(errno));
		return -1;
	}
	ffd[0] = fd[0];
	ffd[1] = fd[1];
	ffd[2] = pd[1];
	*handle = spawn_process(argv, envp, ffd);
	_close(pd[1]);
	if (*handle == -1) {
		file_msg("Error spawning process %s: %s", full_command(argv),
				strerror(errno));
		_close(pd[0]);
		return -1;
	}
	*fderr = pd[0];
	return 0;
}

int
spawn_exists(char *const command, char *const arg)
{
	char *const	argv[3] = {command, arg, NULL};
	int		null = _open("NUL", _O_RDWR | _O_BINARY);
	int		ffd[3] = {null, null, null};
	intptr_t	h;
	int		status;

	if (null < 0)
		return 0;
	h = spawn_process(argv, NULL, ffd);
	_close(null);
	if (h == -1)
		return 0;
	if (_cwait(&status, h, 0) == -1)
		return 0;
	return status == 0;
}

int
spawn_usefd(char *const argv[restrict], char *const envp[restrict],
		int fdin, int fdout)
{
	int		fd[2] = {fdin, fdout};
	int		fderr;
	intptr_t	h;

	if (open_process(argv, envp, fd, &h, &fderr))
		return -1;
	report_stderr(fderr);
	return wait_process(h, 0);
}

int
spawn_popen_fd(char *const argv[restrict], char *const envp[restrict],
		const char *restrict type, int fd)
{
	int		pd[2], fds[2], fderr, parent;
	int		reading = !strcmp(type, "r");
	intptr_t	h;

	if (make_pipe(pd)) {
		file_msg("Command %s, cannot create pipe: %s",
				full_command(argv), strerror(errno));
		return -1;
	}
	/* the process reads from fd or writes to it, and the pipe is the
	   other one */
	if (reading) {
		parent = pd[0];
		fds[0] = fd;
		fds[1] = pd[1];
	} else {
		parent = pd[1];
		fds[0] = pd[0];
		fds[1] = fd;
	}
	if (open_process(argv, envp, fds, &h, &fderr)) {
		_close(pd[0]);
		_close(pd[1]);
		return -1;
	}
	_close(reading ? pd[1] : pd[0]);
	add_info(parent, fderr, h, reading ? 'r' : 'w');
	return parent;
}

int
spawn_popen(char *const argv[restrict], char *const envp[restrict],
		const char *restrict type)
{
	return spawn_popen_fd(argv, envp, type, -1);
}

int
spawn_pclose(int fd)
{
	int		fderr;
	char		type;
	intptr_t	h;

	if (retrieve_info(fd, &fderr, &h, &type)) {
		file_msg("Error retrieving process id of spawned process!");
		return -1;
	}
	if (type == 'w') {
		_close(fd);
		report_stderr(fderr);
		return wait_process(h, 0);
	} else {
		/* closed before all output was read, e.g. after reading an
		   image header: end the process */
		DWORD	avail = 0;
		int	terminated = 0;
		if (PeekNamedPipe((HANDLE)_get_osfhandle(fd), NULL, 0, NULL,
					&avail, NULL) && avail > 0) {
			TerminateProcess((HANDLE)h, 1);
			terminated = 1;
		}
		_close(fd);
		report_stderr(fderr);
		return wait_process(h, terminated);
	}
}
