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

#ifndef U_SPAWN_H
#define U_SPAWN_H

#if defined HAVE_CONFIG_H && !defined VERSION
#include "config.h"		/* restrict */
#endif

extern int	spawn_exists(char *const cmd, char *const arg);
extern int	spawn_usefd(char *const argv[restrict],
			char *const envp[restrict], int fdin, int fdout);
extern int	spawn_popen_fd(char *const argv[restrict],
			char *const envp[restrict],
			const char *restrict type, int fd);
extern int	spawn_popen(char *const argv[restrict],
			char *const envp[restrict], const char *restrict type);
extern int	spawn_pclose(int pd);
#endif
