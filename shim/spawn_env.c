/*
 * spawn_env.c - posix_spawnp() for xfig's u_spawn.c, with PATH.
 *
 * xfig starts fig2dev with a few environment variables only (LANG,
 * XFIGTMPDIR...), not PATH.  fig2dev runs Ghostscript and other helpers
 * through the shell, which then searches /usr/bin:/bin only: Homebrew's
 * gs, in /opt/homebrew/bin, was not found, and export to PDF or PNG failed.
 * Pass PATH on whenever xfig gives an environment without it.
 */
#include <spawn.h>
#include <stdlib.h>
#include <string.h>

int
xq_posix_spawnp(pid_t *pid, const char *file,
		const posix_spawn_file_actions_t *actions,
		const posix_spawnattr_t *attr, char *const argv[],
		char *const envp[])
{
	const char	*path = getenv("PATH");
	size_t		n = 0;
	int		has_path = 0;

	if (!envp || !path)
		return posix_spawnp(pid, file, actions, attr, argv, envp);
	for (; envp[n]; ++n)
		if (!strncmp(envp[n], "PATH=", 5))
			has_path = 1;
	if (has_path)
		return posix_spawnp(pid, file, actions, attr, argv, envp);

	char	**env = malloc((n + 2) * sizeof *env);
	char	*p = malloc(strlen(path) + 6);
	int	ret;
	if (!env || !p) {
		free(env);
		free(p);
		return posix_spawnp(pid, file, actions, attr, argv, envp);
	}
	memcpy(env, envp, n * sizeof *env);
	strcpy(p, "PATH=");
	strcat(p, path);
	env[n] = p;
	env[n + 1] = NULL;
	ret = posix_spawnp(pid, file, actions, attr, argv, env);
	free(p);
	free(env);
	return ret;
}
