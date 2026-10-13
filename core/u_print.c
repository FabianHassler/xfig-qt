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
#include "u_print.h"

#include <errno.h>
#include <fcntl.h>		/* creat(), open() */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <unistd.h>
#include <X11/Intrinsic.h>

#include "resources.h"
#include "mode.h"
#include "f_save.h"
#include "f_util.h"
#include "u_colors.h"
#include "u_spawn.h"
#include "u_create.h"		/* new_string */
#include "w_cursor.h"
#include "w_layers.h"
#include "w_msgpanel.h"
#include "w_util.h"

Boolean	print_all_layers = True;
Boolean	bound_active_layers = False;
Boolean	print_hpgl_pcl_switch;
Boolean	hpgl_specified_font;
Boolean	pdf_pagemode;
int	preview_type;


static void	build_layer_list (char *layers);
static void	append_group (char *list, char *num, int first, int last);

#define	ARGBUF_SIZE	16

/*
 * Break the string given in cmdline at spaces. Spaces quoted by a backslash are
 * retained. Return the substrings in args.
 * Occurences of %f in args are replaced by file.
 * The size of the array args must be given in size.
 * The array f must be of the same size as args.
 * The string returned in arg[i] must be free()'d if f[i] is not 0.
 * Return the number of arguments found in cmdline. If the number returned is
 * larger than size, not the entire cmdline was parsed.
 */
static int
cmdlinetoargarray(char *args[restrict], int size, char *restrict cmdline)
{
	int		i = 0;
	char		*begin;

	if (!cmdline || cmdline[0] == '\0')
		return i;

	begin = cmdline;
	while (*begin != '\0' && *begin == ' ')
		++begin;

	/* empty, or only spaces */
	if (*begin == '\0')
		return i;

	while (*begin != '\0') {
		char	*next = begin;
		/* look for the next unqouted space */
		while ((*next != '\0' && *next != ' ') || (*next == ' ' &&
					next > begin && *(next - 1) == '\\')) {
			if (*next == ' ') {/* next>begin && *(next-1) == '\\' */
				*(next - 1) = ' ';
				/* move forward, to overwrite the backslash */
				memmove(begin + 1, begin, next - begin);
				++begin;
			}
			++next;
		}
		/* next == ' ' || '\0' */
		if (i < size)
			args[i] = begin;

		if (*next == '\0') {
			begin = next;
		} else {
			begin = next + 1;
			*next = '\0';
		}

		/* loop forward over consecutive spaces */
		while (*begin != '\0' && *begin == ' ')
			++begin;
		++i;
	}

	return i;
}

/*
 * Write the export language to args[2].
 */
static void
start_argumentlist(char *arg[restrict], char argbuf[restrict][ARGBUF_SIZE],
		int *restrict a, int *restrict b, char *layers)
{
	/* a refers to the index of arg[], b to the index of argbuf[] */
	*b = -1;
	arg[0] = fig2dev_cmd;
	arg[1] = "-L";
	*a = 2;	/* arg[2] will be the output language */
	if (appres.magnification < 99.99 || appres.magnification > 100.01) {
		int	n;
		arg[++*a] = "-m";
		n = snprintf(argbuf[++*b], ARGBUF_SIZE,
				"%.4g", appres.magnification/100.);
		arg[++*a] = argbuf[*b];
		if (n >= ARGBUF_SIZE)
			file_msg("Unable to write full magnification %.4g, "
					"only %d characters available",
					appres.magnification/100., ARGBUF_SIZE);
	}
	if (!print_all_layers) {
		arg[++*a] = "-D";
		arg[++*a] = layers;
		if (bound_active_layers)
			arg[++*a] = "-K";
	}
}

/*
 * Write the output language "ps" as argument 2 and
 * add up to 15 arguments at the end of the argument list.
 */
static void
addargs_postscript(char *args[restrict], int *restrict a, const char *grid,
			const char *backgrnd)
{
	args[2] = "ps";			/* output language */

	args[++*a] = "-z";
	args[++*a] = paper_sizes[appres.papersize].sname;

	if (strlen(cur_filename)) {
		args[++*a] = "-n";
		args[++*a] = cur_filename;
	}
	args[++*a] = appres.landscape ? "-l" : "-p";
	args[++*a] = "xxx";

	if (appres.correct_font_size)
		args[++*a] = "-F";

	if (!appres.multiple && !appres.flushleft)
		args[++*a] = "-c";

	if (appres.multiple)
		args[++*a] = "-M";

	if (grid[0] && strcasecmp(grid,"none") != 0) {
		args[++*a] = "-G";
		args[++*a] = (char *)grid;
	}

	if (backgrnd[0]) {
		args[++*a] = "-g";
		args[++*a] = (char *)backgrnd;
	}

	args[++*a] = NULL;
}

static void
border_arg(char *args[restrict], char argbuf[const restrict][ARGBUF_SIZE],
		int *restrict a, int *restrict b, int border)
{
	int	n;
	args[++*a] = "-b";
	n = snprintf(argbuf[++*b], ARGBUF_SIZE, "%d", border);
	args[++*a] = argbuf[*b];
	if (n >= ARGBUF_SIZE)
		file_msg("Border thickness %d incorrectly written: %s",
				border, argbuf[*b]);
}

static int
spawn_export_to_fd(char *const args[restrict], char *const envp[restrict],
		int fdout)
{
	int	fd;
	int	stat;

	/* all these commands do their own error reporting */
	if ((fd = spawn_popen_fd(args, envp, "w", fdout)) < 0)
		return -1;
	stat = write_fd(fd);
	fd = spawn_pclose(fd);
	if (stat || fd)
		return -1;
	return 0;
}

static int
spawn_exportcommand(char *const args[restrict], char *const envp[restrict],
		const char *restrict outfile)
{
	int	fdout;

	if ((fdout = open(outfile, O_CREAT | O_WRONLY | O_TRUNC,
					S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP |
					S_IROTH | S_IWOTH)) == -1) {
		file_msg("Cannot open file %s: %s", outfile, strerror(errno));
		return -1;
	}

	if (spawn_export_to_fd(args, envp, fdout)) {
		(void)close(fdout);
		return -1;
	}

	if (close(fdout))
		file_msg("Cannnot close file %s: %s", outfile, strerror(errno));
	return 0;
}

void
print_to_printer(int lpcommand, char *printer, char *backgrnd, float mag,
		Boolean print_all_layers, Boolean bound_active_layers,
		char *grid, char *params)
{
	(void)mag;
	(void)print_all_layers;
	(void)bound_active_layers;
	int	a, b;	/* argument counters */
	int	stat;
	char	layers[PATH_MAX];
	char	*args[18];
	char	argbuf[2][ARGBUF_SIZE];

	/* if the user only wants the active layers, build that list */
	build_layer_list(layers);

	/* fig2dev expects path names of included pictures relative
	   to cur_file_dir */
	change_directory(cur_file_dir);

	start_argumentlist(args, argbuf, &a, &b, layers);

	addargs_postscript(args, &a, grid, backgrnd);

	/* Build up the pipeline from the end, catching printer errors */
	/* These commands already report their errors */
	a = b = stat = -1;
	if ((b = print_spawn_printcmd(lpcommand, NULL, printer, params)) > -1)
		a = spawn_popen_fd(args, NULL, "w", b);
	else
		return;
	if (a > -1) {
		stat = write_fd(a);
	} else {
		(void)spawn_pclose(b);
		return;
	}
	if (!stat) {
		if (emptyname(printer))
			put_msg("Printing on default printer with %s paper size"
					" in %s mode ... done",
				paper_sizes[appres.papersize].sname,
				appres.landscape ? "LANDSCAPE" : "PORTRAIT");
		else
			put_msg("Printing on \"%s\" with %s paper size in %s "
					"mode ... done",
				printer, paper_sizes[appres.papersize].sname,
				appres.landscape ? "LANDSCAPE" : "PORTRAIT");
	}
	(void)spawn_pclose(a);
	(void)spawn_pclose(b);
	app_flush();		/* make sure message gets displayed */
}

/*
 * Write postscript ouptput to fdout.
 * Return 0 on success.
 * Report errors using file_msg().
 */
int
print_to_batchfile(int fdout, const char *restrict backgrnd,
				const char *restrict grid)
{
	int	a, b;	/* argument counters */
	char	layers[PATH_MAX];
	char	*args[18];
	char	argbuf[2][ARGBUF_SIZE];

	/* if the user only wants the active layers, build that list */
	build_layer_list(layers);

	/* fig2dev expects path names of included pictures relative
	   to cur_file_dir */
	change_directory(cur_file_dir);

	start_argumentlist(args, argbuf, &a, &b, layers);

	addargs_postscript(args, &a, grid, backgrnd);

	b = 0;
	if ((a = spawn_popen_fd(args, NULL, "w", fdout)) > -1) {
		b = write_fd(a);
		a = spawn_pclose(a);
	}
	return  a | b;
}

static void
strsub(char *prcmd, char *find, char *repl, char *result, int global)
{
	char *loc;

	do {
		loc = strstr(prcmd, find);
		if (loc == NULL)
			break;

		while ((prcmd != loc) && *prcmd)
			/* copy prcmd into result up to loc */
			*result++ = *prcmd++;
		strcpy(result, repl);
		result += strlen(repl);
		prcmd += strlen(find);
	} while (global);

	strcpy(result, prcmd);
}

static void
pass_environment(char **envp, const char *name[restrict])
{
	extern char	**environ;
	char		**e = environ;
	e = environ;
	while (*name) {
		e = environ;
		while (*e) {
			if (!strncmp(*name, *e, strlen(*name)) &&
					(*e)[strlen(*name)] == '=') {
				*envp++ = *e;
				break;
			}
			++e;
		}
		++name;
	}
	*envp = NULL;
}

/*
 * Export to the current output language cur_exp_lang.
 * Return 0 on success.
 * xoff, yoff, and border are in postscript points (1/72 inch)
 */
int
print_export(char *file, int xoff, int yoff, char *backgrnd, char *transparent,
	      Boolean use_transp_backg, int border, char *grid)
{
	char		layers[PATH_MAX];
	const char	dummy[] = "x";
	char		*outfile, *name;
	char		*save_file_dir;
	char		*tmp_name = NULL;
	char		*suf;
	char		*args[36];
	char		argbuf[5][ARGBUF_SIZE];
	const char	*env_name[] = {"LANG", "LC_ALL", "LC_CTYPE",
					"XFIGTMPDIR", "FIG2DEV_LIBDIR",
					"TMP", "TEMP", NULL};
	static char	*envp_buf[sizeof env_name / sizeof env_name[0]];
	static char	**envp = NULL;
	int		ret = 0;
	int		a;	/* args counter */
	int		b;	/* argbuf counter */
	size_t		bufsize = sizeof argbuf[1];

	/* if file exists, ask if ok */
	if (!ok_to_write(file, "EXPORT"))
		return 1;

	/* only query the environment once, at first invocation */
	if (!envp) {
		envp = envp_buf;
		pass_environment(envp, env_name);
	}

	outfile = strdup(file);
	if (strlen(cur_filename) == 0)
		name = file;
	else
		name = cur_filename;

	put_msg("Exporting to file \"%s\" in %s mode ...     ",
			file, appres.landscape ? "LANDSCAPE" : "PORTRAIT");
	app_flush();		/* make sure message gets displayed */

	/*
	 * print_export is called from w_export.c where the current directory
	 * is set to cur_export_dir, but write_file() writes picture paths
	 * relative to cur_file_dir; Hence, for the spawned fig2dev command to
	 * find the images, set cur_file_dir to cur_export_dir.
	 */
	save_file_dir = strdup(cur_file_dir);
	strcpy(cur_file_dir, cur_export_dir);

	/* if the user only wants the active layers, build that list */
	build_layer_list(layers);

	/* set the numeric locale to C so we get decimal points for numbers */
	setlocale(LC_NUMERIC, "C");

	start_argumentlist(args, argbuf, &a, &b, layers);

	/* args[2] points to the language */
	args[2] = lang_items[cur_exp_lang];

	/* Options common to PostScript, PDF and EPS output */
	if (cur_exp_lang == LANG_PS || cur_exp_lang == LANG_PDF ||
		    cur_exp_lang == LANG_EPS || cur_exp_lang == LANG_PSTEX ||
		    cur_exp_lang == LANG_PDFTEX || cur_exp_lang == LANG_PSPDF ||
		    cur_exp_lang == LANG_PSPDFTEX ) {

		args[++a] = "-F";
		args[++a] = "-n";
		args[++a] = name;

		if (backgrnd[0]) {
			args[++a] = "-g";
			args[++a] = backgrnd;
		}

		if (border > 0)
			border_arg(args, argbuf, &a, &b, border);

		/* any grid spec */
		if (grid[0] && strcasecmp(grid, "none") != 0) {
			args[++a] = "-G";
			args[++a] = grid;
		}

		/* Options common to PS and PDF in PS-mode (not EPS-mode) */
		if (cur_exp_lang == LANG_PS ||
				(cur_exp_lang == LANG_PDF && pdf_pagemode)) {

			if (cur_exp_lang == LANG_PDF /* && pdf_pagemode */)
				args[++a] = "-P";

			args[++a] = "-z";
			args[++a] = paper_sizes[appres.papersize].sname;
			args[++a] = appres.landscape ? "-l" : "-p";
			args[++a] = (char *)dummy;

			if (xoff != 0) {
				int	n;
				args[++a] = "-x";
				n = snprintf(argbuf[++b], bufsize, "%d", xoff);
				args[++a] = argbuf[b];
				if ((size_t)n >= bufsize)
					file_msg("Too large x-shift %d, only "
						 "%zd characters possible",
							xoff, bufsize);
			}
			if (yoff != 0) {
				int	n;
				args[++a] = "-y";
				n = snprintf(argbuf[++b], bufsize, "%d", xoff);
				args[++a] = argbuf[b];
				if ((size_t)n >= bufsize)
					file_msg("Too large y-shift %d, only "
						 "%zd characters possible",
							yoff, bufsize);
			}

			if (appres.multiple) {
				args[++a] = "-M";
				if (appres.overlap)
					args[++a] = "-O";
			} else {
				if (!appres.flushleft)
					args[++a] = "-c";
			}
		}

		if (cur_exp_lang == LANG_PS || cur_exp_lang == LANG_EPS) {
			/* see preview_items in w_export.c */
			switch (preview_type) {
			case 1:				/* ASCII preview */
				args[++a] = "-A";
				break;
			case 2:				/* color tiff preview */
				args[++a] = "-C";
				args[++a] = (char *)dummy;
				break;
			case 3:				/* monochrome tiff */
				args[++a] = "-T";
				break;
			}
		}

		/* Languages which need a second or even third export. */
		if (cur_exp_lang == LANG_PSPDFTEX) {
			size_t		len = strlen(outfile);

			if (!(tmp_name = new_string(len + 4))) {
				ret = 1;
				goto free_outfile;
			}

			/* Options were already set above */
			/* first generate pstex postscript then pdftex PDF.  */
			strsub(outfile, ".", "_", tmp_name, 1);

			/* reset to original locale */
			setlocale(LC_NUMERIC, "");

			/* make it suitable for pstex. */
			strcpy(tmp_name + len, ".eps");
			args[2] = lang_items[LANG_PSTEX];
			args[++a] = NULL;
			spawn_exportcommand(args, envp, tmp_name);

			/* make it suitable for pdftex. */
			strcpy(tmp_name + len, ".pdf");
			args[2] = lang_items[LANG_PDFTEX];
			spawn_exportcommand(args, envp, tmp_name);

			/* and then the tex code. */
			setlocale(LC_NUMERIC, "C");
			start_argumentlist(args, argbuf, &a, &b, layers);
			args[2] = "pstex_t";
			tmp_name[len] = '\0';
			args[++a] = "-p";
			args[++a] = tmp_name;

		/* PSTEX and PDFTEX */
		} else if (cur_exp_lang == LANG_PSTEX ||
				cur_exp_lang == LANG_PDFTEX) {
			size_t	len = strlen(outfile);
			char	*s;

			/* Options were already set above
			    - output the first file */
			args[++a] = NULL;
			spawn_exportcommand(args, envp, outfile);

			/* now the text part */
			/* add "_t" to the output filename */
			if (!(tmp_name = new_string(len))) {
				ret = 1;
				goto free_outfile;
			}
			memcpy(tmp_name, outfile, len + 1);
			if (!(outfile = realloc(s = outfile, len + 3))) {
				ret = 1;
				outfile = s;	/* for free(outfile) below */
				goto free_tmp_name;
			}
			strcpy(outfile + len, "_t");
			setlocale(LC_NUMERIC, "C");
			start_argumentlist(args, argbuf, &a, &b, layers);
			args[2] = "pstex_t";
			args[++a] = "-p";
			args[++a] = tmp_name;

			if (border > 0)
				border_arg(args, argbuf, &a, &b, border);

		/* PSPDF */
		} else if (cur_exp_lang == LANG_PSPDF) {

			/* Output first file */
			args[2] = lang_items[LANG_EPS];
			args[++a] = NULL;
			spawn_exportcommand(args, envp, outfile);

			setlocale(LC_NUMERIC, "C");
			start_argumentlist(args, argbuf, &a, &b, layers);
			args[2] = lang_items[LANG_PDF];

			/* any grid spec */
			if (grid[0] && strcasecmp(grid, "none") != 0) {
				args[++a] = "-G";
				args[++a] = grid;
			}

			if (border > 0)
				border_arg(args, argbuf, &a, &b, border);
			args[++a] = "-F";
			args[++a] = "-n";
			args[++a] = name;

			if (backgrnd[0]) {
				args[++a] = "-g";
				args[++a] = backgrnd;
			}

			/* now change the output file name to xxx.pdf */
			/* strip off current suffix, if any */
			if ((suf = strrchr(outfile, '.'))) {
				size_t	len = strlen(outfile);
				if (len - (suf-outfile) < 4) {
					if (!(tmp_name = realloc(outfile,
								len + 5))) {
					       ret = 1;
					       goto free_outfile;
				       }
				} else {
					tmp_name = outfile;
				}
				sprintf(tmp_name + (suf - outfile), ".pdf");
			} else {	/* no suffix, add one */
				size_t	len = strlen(outfile);
				if (!(tmp_name = realloc(outfile, len + 5))) {
					ret = 1;
					goto free_outfile;
				}
				strcpy(tmp_name + len, ".pdf");
			}
			if (tmp_name != outfile)
				outfile = tmp_name;
			tmp_name = NULL;  /* otherwise double-free */
		}

	} else if (cur_exp_lang == LANG_IBMGL) {

		if (hpgl_specified_font)
			args[++a] = "-F";

		if (print_hpgl_pcl_switch)
			args[++a] = "-k";

		args[++a] = "-z";
		args[++a] = paper_sizes[appres.papersize].sname;

		if (!appres.landscape)
			args[++a] = "-P";

	} else if (cur_exp_lang == LANG_MAP) {

		/* HTML map needs border option */
		if (border > 0)
			border_arg(args, argbuf, &a, &b, border);

		args[++a] = outfile;

	} else if (cur_exp_lang == LANG_PCX || cur_exp_lang == LANG_PNG ||
			cur_exp_lang == LANG_TIFF || cur_exp_lang == LANG_XBM ||
#ifdef USE_XPM
			cur_exp_lang == LANG_XPM ||
#endif
			cur_exp_lang == LANG_PPM || cur_exp_lang == LANG_JPEG ||
			cur_exp_lang == LANG_GIF) {

		/* GIF must come before giving background option */
		if (cur_exp_lang == LANG_GIF) {
			/* select the transparent color, if any */
			if (transparent) {
				/* if user wants background transparent, set
				   the background to the transparrent color */
				if (use_transp_backg)
					backgrnd = transparent;
				args[++a] = "-t";
				args[++a] = transparent;
			}
		} else if (cur_exp_lang == LANG_JPEG) {
			/* set the image quality for JPEG export */
			/*
			 * DEF_JPEG_QUALITY is defined in resources.h. Make
			 * sure, it stays in sync with the value for
			 * jpeg_quality given in fig2dev/dev/genbitmaps.c
			 */
			if (appres.jpeg_quality != DEF_JPEG_QUALITY) {
				args[++a] = "-q";
				(void)snprintf(argbuf[++b], bufsize,
						"%d", appres.jpeg_quality);
				args[++a] = argbuf[b];
			}
		}

		/* bitmap formats need border option */
		if (border > 0)
			border_arg(args, argbuf, &a, &b, border);

		if (appres.smooth_factor) {
			args[++a] = "-S";
			(void)snprintf(argbuf[++b], bufsize,
					"%d", appres.smooth_factor);
			args[++a] = argbuf[b];
		}

		args[++a] = "-F";

		if (backgrnd[0]) {
			args[++a] = "-g";
			args[++a] = backgrnd;
		}

	/* TK or  PERL/TK */
	} else if (cur_exp_lang == LANG_TK || cur_exp_lang == LANG_PTK) {

		if (backgrnd[0]) {
			args[++a] = "-g";
			args[++a] = backgrnd;
		}

	} else if (cur_exp_lang == LANG_DXF) {

		if (!appres.landscape)
			args[++a] = "-P";

	}
	/* Nothing to do for everything else */

	/* make a busy cursor */
	set_temp_cursor(wait_cursor);

	/* reset to original locale */
	setlocale(LC_NUMERIC, "");

	/* now execute fig2dev */
	args[++a] = NULL;
	if (!spawn_exportcommand(args, envp, outfile))
		put_msg("Export to \"%s\" done", file);
	else
		put_msg("Export to \"%s\" failed", file);

	/* and reset the cursor */
	reset_cursor();

	/* free tempnames */
free_tmp_name:
	if (tmp_name)
		free(tmp_name);
free_outfile:
	strcpy(cur_file_dir, save_file_dir);
	free(save_file_dir);
	free(outfile);
	return ret;
}

int
print_spawn_printcmd(int lpcommand, const char *restrict file,
			const char *restrict printer, char *restrict params)
{
	int		a, n;
	char		*pargs[16];
	char		**nargs = pargs;
	/* to correspond to print_command_items[] in w_print.c */
	const char	*lpcmd[2] = {"lp", "lpr"};
	const char	*lparg[2] = {"-d", "-P"};
	/* see man lp(1) for the environment variables lp may query */
	const char	*env_name[] = {"LANG", "LC_ALL", "LC_CTYPE",
					"LC_MESSAGES", "LC_TIME", "LPDEST",
					"NLSPATH", "PRINTER", "TZ", "NULL"};
	static char	*envp_buf[sizeof env_name / sizeof env_name[0]];
	static char	**envp = NULL;

	/* only query the environment once, at first invocation */
	if (!envp) {
		envp = envp_buf;
		pass_environment(envp, env_name);
	}

	pargs[a = 0] = (char *)lpcmd[lpcommand];
	if (printer && printer[0] != '\0') {
		pargs[++a] = (char *)lparg[lpcommand];
		pargs[++a] = (char *)printer;
	}

	++a;
	n = cmdlinetoargarray(pargs + a , sizeof pargs/sizeof pargs[0] - a - 2,
		       params);
	if ((size_t)n > sizeof pargs/sizeof pargs[0] - a - 2) {
		if (!(nargs = malloc(n * sizeof(char *)))) {
			file_msg("Unable to print, running out of memory");
			file_msg("Print parameters: %s", params);
			return -1;
		}
		n = cmdlinetoargarray(nargs, n, params);
	}
	a += n - 1;		/* n might be 0 */

	if (file && file[0] != '\0') {
		nargs[++a] = (char *)file;
		nargs[++a] = NULL;
		return spawn_usefd(nargs, envp, -1, -1);
	} else {
		nargs[++a] = NULL;
		return spawn_popen(nargs, envp, "w");
	}
}

/*
   make an rgb string from color (e.g. #31ab12)
   if the color is < 0, make empty string
*/
void
make_rgb_string(int color, char *rgb_string)
{
	if (color >= 0) {
		sprintf(rgb_string, "#%02x%02x%02x",
				getred(color)>>8,
				getgreen(color)>>8,
				getblue(color)>>8);
	} else {
		rgb_string[0] = '\0';	/* no background wanted by user */
	}
}

/* make up the -D option to fig2dev if user wants to print only active layers */

static void
build_layer_list(char *layers)
{
	char	list[PATH_MAX], notlist[PATH_MAX], num[10];
	int	layer, len, notlen;
	int	firstyes, lastyes = 0, firstno, lastno = 0;

	layers[0] = '\0';

	if (print_all_layers)
		return;

	list[0] = notlist[0] = '\0';
	len = notlen = 0;

	/* build up two lists - layers TO print and layers to NOT print */
	/* use the smaller of the two in the final command */

	firstyes = firstno = -1;
	for (layer=min_depth; layer<=max_depth; layer++) {
		if (active_layers[layer] && object_depths[layer]) {
			if (firstyes == -1)
				firstyes = lastyes = layer;
			/* see if there is a contiguous set */
			if (layer-lastyes <= 1) {
				lastyes = layer;	/* so far, yes */
				continue;
			}
			append_group(list, num, firstyes, lastyes);
			firstyes = lastyes = layer;
			if (len+strlen(list) >= PATH_MAX-5)
				continue;   /* list is too long, don't append */
			strcat(list,num);
			len += strlen(list)+1;
		} else if (object_depths[layer]) {
			if (firstno == -1)
				firstno = lastno = layer;
			/* see if there is a contiguous set */
			if (layer-lastno <= 1) {
				lastno = layer;		/* so far, yes */
				continue;
			}
			if (firstno == -1)
				firstno = layer;
			append_group(notlist, num, firstno, lastno);
			firstno = lastno = layer;
			if (notlen+strlen(notlist) >= PATH_MAX-5)
				continue;   /* list is too long, don't append */
			strcat(notlist,num);
			notlen += strlen(notlist)+1;
		}
	}
	if (firstyes != -1) {
		append_group(list, num, firstyes, lastyes);
		if (len+strlen(list) < PATH_MAX-5) {
			strcat(list,num);
			len += strlen(list)+1;
		}
	}
	if (firstno != -1) {
		append_group(notlist, num, firstno, lastno);
		if (notlen+strlen(notlist) < PATH_MAX-5) {
			strcat(notlist,num);
			notlen += strlen(notlist)+1;
		}
	}
	if (len < notlen && firstyes != -1) {
		/* use list of layers TO print */
		sprintf(layers, "+%s", list);
	} else if (firstno != -1){
		/* use list of layers to NOT print */
		sprintf(layers, "-%s", notlist);
	}
}

void
append_group(char *list, char *num, int first, int last)
{
	if (list[0])
		strcat(list,",");
	if (first==last)
		sprintf(num,"%0d",first);
	else
		sprintf(num,"%0d:%d",first,last);
}
