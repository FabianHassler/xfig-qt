/*
 * FIG : Facility for Interactive Generation of figures
 * Copyright (c) 1985-1988 by Supoj Sutanthavibul
 * Parts Copyright (c) 1989-2015 by Brian V. Smith
 * Parts Copyright (c) 1991 by Paul King
 * Parts Copyright (c) 2016-2024 by Thomas Loimer
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <poll.h>
#include <unistd.h>		/* close() */
#include <signal.h>		/* SIGHUP */
#include <stdio.h>
#include <stdlib.h>		/* malloc() */
#include <string.h>		/* strerror() */
#include <sys/stat.h>		/* mode_t, in the open() call */
#include <sys/wait.h>
#ifdef HAVE_POSIX_SPAWNP
#include <spawn.h>
#endif

#include "w_msgpanel.h"

/*
 * The first element is statically allocated.
 * All other elements, if they ever appear, must be allocated on the heap.
 */
static struct process_info {
	int			fd;
	int			fderr;
	pid_t			pid;
	char			type;
	struct process_info	*next;
} process_info = { -1, 0, -1, '\0', NULL };

static struct process_info *const first = &process_info;


/*
 * Fill the first or add an element to the linked list of process information.
 * The elements are identified based on the file descriptor fd.
 */
static void
add_info(int fd, int fderr, pid_t pid, char type)
{
	if (first->fd == -1) {
		first->fd = fd;
		first->fderr = fderr;
		first->pid = pid;
		first->type = type;
	} else {
		struct process_info	*info = first;
		while (info->next)
			info = info->next;
		if (!(info->next = malloc(sizeof(struct process_info))))
			return;
		info = info->next;
		info->fd = fd;
		info->fderr = fderr;
		info->pid = pid;
		info->type = type;
		info->next = NULL;
	}
}

/*
 * Input: fd
 * Outputs: fderr, pid
 */
static int
retrieve_info(int fd, int *fderr, pid_t *pid, char *type)
{
	if (first->fd == fd) {
		*fderr = first->fderr;
		*pid = first->pid;
		*type = first->type;
		if (first->next) {
			struct process_info	*info = first->next;
			first->fd = info->fd;
			first->fderr = info->fderr;
			first->pid = info->pid;
			first->type = info->type;
			first->next = info->next;
			free(info);
		} else {
			first->fd = -1;
		}
		return 0;
	} else {
		struct process_info	*info = first->next;
		struct process_info	*prev = first;
		if (!info)
			return -1;
		while (info->fd != fd && info->next) {
			prev = info;
			info = info->next;
		}
		if (info->fd == fd) {
			*fderr = info->fderr;
			*pid = info->pid;
			*type = info->type;
			prev->next = info->next;
			free(info);
			return 0;
		}
		return -1;
	}
}

/*
 * Return a pointer to a statically allocated buffer containing
 * the concatenated argument list in argv[].
 */
static char *
full_command(char *const argv[restrict])
{
	static char	cmd_buf[256];
	char		*const *c = argv;
	size_t		pos = 0;
	size_t		len;

	while (*c != NULL && (pos + (len = strlen(*c)) < sizeof cmd_buf)) {
		memcpy(cmd_buf + pos, *c, len);
		pos += len;
		cmd_buf[pos++] = ' ';
		++c;
	}
	cmd_buf[pos <= sizeof cmd_buf ? pos - 1 : pos - len - 1] = '\0';
	return cmd_buf;
}

/*
 * Spawn the process argv[0], with the NULL-terminated argument list argv and
 * the environment given by the NULL-terminated array envp.
 * If any of the fd[] is not -1, re-direct stdin, stdout or stderr to the file
 * descriptor given in fd[0], fd[1] or fd[2], respectively.
 * The file descriptors given in cfd are closed in the spawned process.
 * At least one of all file descriptors in fd[] and cfd[] should be valid.
 * Return 0 on success, an errno on failure.
 */
static int
spawn_process(pid_t *restrict pid, char *const argv[restrict],
		char *const envp[restrict], int fd[3], int cfd[2])
{
	int		i;

#ifdef HAVE_POSIX_SPAWNP
	posix_spawn_file_actions_t	file_actions;

	posix_spawn_file_actions_init(&file_actions);
	if (cfd[0] > -1)
		posix_spawn_file_actions_addclose(&file_actions, cfd[0]);
	if (cfd[1] > -1)
		posix_spawn_file_actions_addclose(&file_actions, cfd[1]);
	for (i = 0; i < 3; ++i) {
		if (fd[i] > -1) {
			/* 0 must be stdin, 1 stdout, and 2 stderr */
			posix_spawn_file_actions_adddup2(
					&file_actions, fd[i], i);
			posix_spawn_file_actions_addclose(
					&file_actions, fd[i]);
		}
	}

	return posix_spawnp(pid, argv[0], &file_actions, NULL, argv, envp);

#else	/* HAVE_POSIX_SPAWNP undefined: fork() and exec() */
	*pid = fork();
	if (*pid == -1)
		return errno;
	if (*pid == 0) {
		/* the child */
		if (cfd[0] > -1)
			close(cfd[0]);
		if (cfd[1] > -1)
			close(cfd[1]);
		for (i = 0; i < 3; ++i) {
			if (fd[i] > -1) {
				if (dup2(fd[i], i) == -1)
					return errno;
				close(fd[i]);
			}
		}
		if (execvpe(argv[0], argv, envp) == -1)
			return errno;;
	}

	/* the parent */
	return 0;
#endif /* HAVE_POSIX_SPAWNP */
}

/*
 * Spawn the process argv[0] with the NULL-terminated arguments argv.
 * Search PATH for the command given in argv[0].
 * If either of the fd[i] is non-negative, redirect stdin (fd[0]) and/or stdout
 * (fd[1]) in the spawned process to the open file descriptor fd[i].
 * Close the file descriptor cd in the spawned process.
 * Standard error is captured in a buffer and reported to the user.
 * Return the process id in pid and a file descriptor in fderr from which
 * to read stderr of the spawned process.
 * Return 0 on success.
 * On error, return -1, and output an error message.
 */
static int
open_process(char *const argv[restrict], char *const envp[restrict],
		int fd[2], int cd, pid_t *pid, int *fderr)
{
	int		i;
	int		pd[2];
	int		ffd[3] = {-1, -1, -1};
	int		cfd[2] = {cd, -1};

	if (pipe(pd)) {
		file_msg("Spawning %s, cannot create pipe: %s",
				full_command(argv), strerror(errno));
		return -1;
	}

	/* for example, if fd[1] = fdout, redirect stdout to fdout */
	for (i = 0; i < 2; ++i) {
		if (fd[i] > -1)
			ffd[i] = fd[i];
	}
	ffd[2] = pd[1];
	cfd[1] = pd[0];
	/* and return the read end of the pipe for polling */
	*fderr = pd[0];
	if ((i = spawn_process(pid, argv, envp, ffd, cfd))) {
		file_msg("Error spawning process %s: %s",
				full_command(argv), strerror(i));
		return -1;
	}

	/* close the write end of the pipe to stderr */
	(void)close(pd[1]);
	return 0;
}

static void
closefderr(int fderr)
{
	if (close(fderr))
		file_msg("Error closing stderr: %s", strerror(errno));
}

static int
wait_pid(pid_t pid, int ignore_signal)
{
	int	ret;

	if (waitpid(pid, &ret, 0) == -1) {
		file_msg("Error waiting for spawned process: %s",
				strerror(errno));
		return -1;
	}

	if (WIFEXITED(ret)) {
		ret = WEXITSTATUS(ret);
	} else {
		ret = WTERMSIG(ret);
		if (ignore_signal && ret == ignore_signal)
			ret = 0;
		else
			file_msg("Spawned process interrupted by signal %d",
					ret);
	}
	return ret;
}

/*
 * Poll fderr and output the first 255 bytes received on fderr.
 * Continue reading until fderr is empty.
 * If a process did not write to stderr, anyhow a POLLHUP is detected when the
 * process finishes.
 */
static void
poll_fderr(int fderr, int timeout)
{
	struct pollfd	fds;

	fds.fd = fderr;
	fds.events = POLLIN;		/* POLLHUP is anyhow reported */
	fds.revents = 0;

	timeout = poll(&fds, 1, timeout); /* re-use timeout as return code */
	if (timeout == 0) {
		return;
	} else if (timeout < 0) {
		file_msg("Error polling stderr: %s", strerror(errno));
		return;
	}
	/* poll > 0 */
	if (fds.revents & POLLIN) {
		/* read the first 255 bytes, not more */
		char		buf[256];
		ssize_t		n = 0;
		size_t		num = 0;

		while ((n = read(fderr, buf + num, sizeof buf - 1 - num)) > 0 &&
				(num += n) < sizeof buf - 1)
			;
		if (num > 0) {
			buf[num] = '\0';
			file_msg("Error message from spawned process: %s", buf);
		}
		if (n == -1) {
			file_msg("Error reading error message: %s",
					strerror(errno));
		} else if (n == 0 && num == sizeof buf - 1) {
			/* more data is available; Read the rest,
			   to not cause Broken pipe (SIGPIPE). */
			while (read(fderr, buf, sizeof buf) > 0)
				;
		}
	}
	if (fds.revents & (POLLERR | POLLNVAL)) {
		file_msg("Error polling stderr:%s%s%s",
				fds.revents & POLLERR ? " POLLERR" : "",
				fds.revents & POLLERR && fds.revents & POLLNVAL?
					"," : "" ,
				fds.revents & POLLNVAL ?
					" file descriptor not open" : "");
	}
}

/*
 * Spawn command, with the argument arg. Search PATH for command.
 * Do not pass the environment to the spawned command.
 * Silently consume any output to stdout or stderr from the command.
 * Return 1 if the command exists, 0 if not or if an error occurs.
 */
int
spawn_exists(char *const command, char *const arg)
{
	int		ret;
	int		pderr[2];
	int		pdout[2];
	int		ffd[3];
	int		cfd[2];
	pid_t		pid;
	char *const	argv[3] = {command, arg, NULL};
	struct pollfd	fds[2];

	/* create the pipes */
	if (pipe(pderr)) {
		file_msg("Testing command %s, cannot create pipe: %s",
				command, strerror(errno));
		return 0;
	}

	if (pipe(pdout)) {
		file_msg("Testing command %s, cannot create pipe: %s",
				command, strerror(errno));
		return 0;
	}

	/* redirect stdout and stderr in the process to our pipes */
	ffd[0] = -1;
	ffd[1] = pdout[1];
	ffd[2] = pderr[1];
	cfd[0] = pdout[0];
	cfd[1] = pderr[0];

	ret = spawn_process(&pid, argv, NULL, ffd, cfd);
	(void)close(pdout[1]);
	(void)close(pderr[1]);

	if (ret) {
		(void)close(pdout[0]);
		(void)close(pderr[0]);
		return 0;	/* return, if the command does not exist */
	}

	/* need to poll, cannot read from stdout and stderr at the same time */
	fds[0].fd = pdout[0];
	fds[1].fd = pderr[0];
	fds[0].events = fds[1].events = POLLIN;	/* POLLHUP is anyhow reported */
	fds[0].revents = fds[1].revents = 0;

	while ((ret = poll(fds, 1, -1 /* no timeout */)) > 0) {
		char	buf[256];

		/* read all output */
		if (fds[0].revents & POLLIN)
			while (read(fds[0].fd, buf, sizeof buf) > 0)
				;
		if (fds[1].revents & POLLIN)
			while (read(fds[1].fd, buf, sizeof buf) > 0)
				;
		/* a POLLHUP probably means that both outputs are closed */
		if (fds[0].revents & POLLHUP || fds[1].revents & POLLHUP)
			break;

		if (fds[0].revents & (POLLERR | POLLNVAL))
			file_msg("Error polling stdout of %s: %d",
					command, fds[0].revents);
		if (fds[1].revents & (POLLERR | POLLNVAL))
			file_msg("Error polling stderr of %s: %d",
					command, fds[1].revents);
	}

	if (ret < 0)
		file_msg("Testing command %s, polling error: %s",
				command, strerror(errno));
	if (close(fds[0].fd))
		file_msg("Testing command %s, error closing stderr: %s",
				command, strerror(errno));
	if (close(fds[1].fd))
		file_msg("Testing command %s, error closing stdout: %s",
				command, strerror(errno));

	if (waitpid(pid, &ret, 0) == -1) {
		file_msg("Error waiting for return of command %s: %s",
				command, strerror(errno));
		return 0;
	}

	if (WIFEXITED(ret)) {
		ret = WEXITSTATUS(ret);
	} else {
		ret = WTERMSIG(ret);
		file_msg("Command %s interrupted by signal %d", command, ret);
	}

	if (ret == 0)
		return 1; /* command found */
	else
		return 0;
}

/*
 * Spawn the process argv[0] with the NULL-terminated arguments argv and
 * the environment given by the NULL-terminated array envp.
 * Search PATH for the command given in argv[0].
 * If either of the file descriptors fdin or fdout is non-negative,
 * the spawned process reads from fdin and writes to fdout.
 * Both must be open, if valid, and remain open.
 * Standard error is captured in a buffer and reported to the user.
 * Return the exit status of the spawned process.
 */
int
spawn_usefd(char *const argv[restrict], char *const envp[restrict],
		int fdin, int fdout)
{
	int	fderr;
	int	fd[2] = {fdin, fdout};
	pid_t	pid;

	if (open_process(argv, envp, fd, -1, &pid, &fderr))
		return -1;
	poll_fderr(fderr, -1);
	closefderr(fderr);
	return wait_pid(pid, 0);
}

/*
 * Spawn a process and open a pipe, either for reading ("r") or for writing.
 * Return a file desrciptor for reading the output of the process, or for
 * writing to stdin of the process. If fd is non-negative, the process itself
 * will read from fd ("r") or write to it.
 * Return -1 in case of error.
 */
int
spawn_popen_fd(char *const argv[restrict], char *const envp[restrict],
		const char *restrict type, int fd)
{
	int	fderr;
	int	parent;
	int	child;
	int	pd[2];
	pid_t	pid;

	if (pipe(pd)) {
		file_msg("Command %s, cannot create pipe: %s",
				full_command(argv), strerror(errno));
		return -1;
	}
	/* re-use pd as input to open_process */
	if (!strcmp(type, "r")) {
		parent = pd[0];
		pd[0] = fd;
		child = 1;
	} else {
		parent = pd[1];
		pd[1] = fd;
		child = 0;
	}

	if (open_process(argv, envp, pd, parent, &pid, &fderr)) {
		close(parent);
		parent = -1;
	} else {
		/*
		 * The closing procedure differs, depending on whether data is
		 * written to the spawned process or read from it.
		 */
		if (child == 1)
			add_info(parent, fderr, pid, 'r');
		else
			add_info(parent, fderr, pid, 'w');
	}
	close(pd[child]);

	return parent;
}

/*
 * Spawn a process and open a pipe, either for reading ("r") or for writing.
 * Return a file desrciptor for reading the output of the process, or for
 * writing to stdin of the process.
 * Return -1 in case of error.
 * Example:
 *   fd = spawn_popen("date", NULL, "r");  char buf[256];
 *   if (read(fd,buf,sizeof buf) < sizeof buf && read(fd,buf,sizeof buf) == 0)
 *		spawn_pclose(fd);
 */
int
spawn_popen(char *const argv[restrict], char *const envp[restrict],
		const char *restrict type)
{
	return spawn_popen_fd(argv, envp, type, -1);
}

/*
 * Terminate the process that reads or writes to the other end of fd,
 * and close fd.
 */
int
spawn_pclose(int fd)
{
	char		type;
	int		fderr;
	pid_t		pid;

	if (retrieve_info(fd, &fderr, &pid, &type)) {
		file_msg("Error retrieving process id of spawned process!");
		file_msg("Can you reproduce this error?");
		return -1;
	}

	/* Closing procedures differ for reading and writing pipes. */
	if (type == 'w') {
		if (close(fd))
			file_msg("Error closing connection to spawned process:"
					" %s", strerror(errno));
		poll_fderr(fderr, -1);
		closefderr(fderr);
		return wait_pid(pid, 0);
	} else { /* type == 'r' */
		int		ret;
		struct pollfd	pfds = {fd, POLLIN, 0};

		/*
		 * The pipe might be closed prematurely, e.g., when reading only
		 * the first bytes of the header to determine the image type. In
		 * that case, kill the process. SIGUSR1 was also tried, but unxz
		 * exited with 1 and did not report termination by a signal.
		 */
		poll_fderr(fderr, 0);
		ret = poll(&pfds, 1, 0);
		if (ret < 0 || (ret > 0 && pfds.revents & POLLIN
						&& !(pfds.revents & POLLHUP))) {
			(void)kill(pid, ret = SIGHUP);
		} /* else: ret == 0 */
		ret = wait_pid(pid, ret);
		closefderr(fderr);
		if (close(fd))
			file_msg("Error closing connection to spawned process:"
					" %s", strerror(errno));
		return ret;
	}
}
