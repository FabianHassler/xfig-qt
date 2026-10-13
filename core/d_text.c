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
#include "d_text.h"

#include <errno.h>
#include <math.h>
#include <limits.h>
#include <signal.h>		/* kill, SIGTERM */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <unistd.h>
#include <sys/wait.h>		/* waitpid() */
#include <X11/keysym.h>
#include <X11/IntrinsicP.h>    /* includes X11/Xlib.h, which includes X11/X.h */
#include <X11/Xft/Xft.h>

#include "resources.h"
#include "mode.h"
#include "object.h"
#include "paintop.h"
#include "d_text.h"
#include "u_bound.h"
#include "u_colors.h"
#include "u_create.h"
#include "u_fonts.h"
#include "u_free.h"
#include "u_list.h"
#include "u_markers.h"
#include "u_redraw.h"
#include "u_search.h"
#include "u_undo.h"
#include "w_canvas.h"
#include "w_cursor.h"
#include "w_cmdpanel.h"
#include "w_drawprim.h"
#include "w_mousefun.h"
#include "w_msgpanel.h"
#include "w_zoom.h"
#include "xfig_math.h"


/* EXPORTS */
int		work_font;
XFontStruct	*canvas_font;


/* LOCALS */

#define CTRL_A	'\001'		/* move to beginning of text */
#define CTRL_B	'\002'		/* move back one char */
#define CTRL_D	'\004'		/* delete right of cursor */
#define CTRL_E	'\005'		/* move to end of text */
#define CTRL_F	'\006'		/* move forward one char */
#define CTRL_H	'\010'		/* backspace */
#define CTRL_K	'\013'		/* kill to end of text */
#ifdef SEL_TEXT
#define CTRL_W	'\027'		/* delete selected text */
#endif /* SEL_TEXT */
#define CTRL_HAT  '\036'	/* start superscript or end subscript */
#define CTRL_UNDERSCORE  '\037'	/* start subscript or end superscript */

#define	MAX_SUPSUB 4			/* max number of nested super/subscripts */
#define CSUB_FRAC 0.75			/* fraction of current char size for super/subscript */
#define PSUB_FRAC (1.0-CSUB_FRAC)	/* amount of up/down shift */

#define NL	'\n'
#define ESC	'\033'
#define CR	'\r'
#define CTRL_X	24
#define SP	' '
#define DEL	127

#define		BUF_SIZE	400

static char	first_char_old_t;
static char	prefix[BUF_SIZE];	/* part of string left of mouse click */
static int	leng_prefix;
static int	start_suffix;
static F_pos	char_ht;		/* vector to next line */
static int	base_x, base_y;
static int	supersub;		/* < 0 = currently subscripted,
					   > 0 = superscripted */
static F_pos	heights[MAX_SUPSUB];	/* keep prev char heights when
					   super/subscripting */

static F_pos	ascent, descent;	/* vectors up- or down from baseline */
static int	orig_x, orig_y;		/* to position next line */
static F_pos	orig_ht;		/* to advance to next line */
static struct xfig_font	*canvas_zoomed_xffont;

static Boolean	is_newline;
static float	work_fontsize;
static float	orig_fontsize;
static int	work_flags, work_psflag, work_textjust, work_depth;
static Color	work_textcolor;
static float	work_angle;		/* in RADIANS */

static void	begin_utf8char(unsigned char *str, int *pos);
static void	end_utf8char(unsigned char *str, int *pos);
static void	finish_n_start(int x, int y);
static void	init_text_input(int x, int y), cancel_text_input(void);
static F_text	*new_text(int len, char *string);

static void	new_text_line(void);
static void	overlay_text_input(int x, int y);
static int	split_at_cursor(F_text *t, int base_x, int base_y,
					int *x, int *y, int *start_suffix);
static void	draw_cursor(int x, int y);
static void	initialize_char_handler(void);
static void	terminate_char_handler(void);
static void	turn_on_blinking_cursor(int x, int y);
static void	turn_off_blinking_cursor(void);
static void	move_blinking_cursor(int x, int y);

XIM		xim_im = NULL;
XIC		xim_ic = NULL;
XIMStyle	xim_style = 0;
Boolean		xim_active = False;

static int	save_base_x, save_base_y;

static void	xim_set_spot();

static pid_t	preedit_pid = -1;
static char	preedit_filename[PATH_MAX] = "";
static void	open_preedit_proc(), close_preedit_proc(), paste_preedit_proc();
static Boolean	is_preedit_running();

/********************************************************/
/*							*/
/*			Procedures			*/
/*							*/
/********************************************************/


void
text_drawing_selected(void)
{
	canvas_kbd_proc = null_proc;
	canvas_locmove_proc = null_proc;
	canvas_middlebut_proc = null_proc;
	canvas_leftbut_proc = init_text_input;
	canvas_rightbut_proc = null_proc;
	set_mousefun("position cursor", "", "", "", "", "");
	if (appres.international && strlen(appres.text_preedit) != 0) {
		if (is_preedit_running()) {
			canvas_middlebut_proc = paste_preedit_proc;
			canvas_rightbut_proc = close_preedit_proc;
			set_mousefun("position cursor", "paste pre-edit",
					"close pre-edit", "", "", "");
		} else {
			canvas_rightbut_proc = open_preedit_proc;
			set_mousefun("position cursor", "", "open pre-edit",
							"", "", "");
		}
	}
	reset_action_on();
	clear_mousefun_kbd();

	set_cursor(text_cursor);
	is_newline = False;
}

static void
commit_current_text(void)
{
	if (old_t) {
		if (first_char_old_t) {
			old_t->cstring[0] = first_char_old_t;
			first_char_old_t = '\0';
		}
		if (cur_t->cstring[0] == '\0')
			delete_text(old_t);
		else
			change_text(old_t, cur_t);
		old_t = NULL;
	} else { /* !old_t */
		if (cur_t->cstring[0] != '\0')
			add_text(cur_t);
	}
	cur_t = NULL;
}

static void
finish_n_start(int x, int y)
{
	reset_action_on();
	terminate_char_handler();
	commit_current_text();
	/* reset text size after any super/subscripting */
	work_fontsize = cur_fontsize;
	supersub = 0;
	is_newline = False;
	init_text_input(x, y);
}

void
finish_text_input(int x, int y, int shift)
{
	(void)x;
	(void)y;
	if (shift) {
		paste_primary_selection();
		return;
	}
	reset_action_on();
	terminate_char_handler();
	commit_current_text();
	text_drawing_selected();
	/* reset text size after any super/subscripting */
	work_fontsize = cur_fontsize;
	/* reset super/subscript */
	supersub = 0;
	draw_mousefun_canvas();
}

static void
cancel_text_input(void)
{
	int	a,b,c,d, e,f,g,h;
	/* erase the current text */
	text_bound(cur_t, &a, &b, &c, &d);
	/* Make current text invisible and redraw. This gave better results
	   than erase_box(a,b,c,d); Also, a redisplay_region(e,f,g,h) below
	   instead of redisplay_regions(..) was not sufficient. */
	cur_t->cstring[0] = '\0';
	redisplay_text(cur_t);
	free_text(&cur_t);
	cur_t = NULL;
	/* old_t points to the text object in the objects list.
	   Recover its text string which was made invisible for redrawing. */
	if (old_t) {
		if (first_char_old_t) {
			old_t->cstring[0] = first_char_old_t;
			first_char_old_t = '\0';
		}
		text_bound(old_t, &e, &f, &g, &h);
		redisplay_regions(a,b,c,d, e,f,g,h);
		old_t = NULL;
	}
	/* reset text size after any super/subscripting */
	work_fontsize = cur_fontsize;
	/* reset super/subscript */
	supersub = 0;
	terminate_char_handler();
	reset_action_on();

	text_drawing_selected();
	draw_mousefun_canvas();
}

static void
new_text_line(void)
{
	commit_current_text();
	turn_off_blinking_cursor();

	/* restore x,y to point where user clicked first text or start
	   of text if clicked on existing text */
	cur_x = orig_x;
	cur_y = orig_y;
	/* restore orig char height */
	char_ht = orig_ht;

	/* advance to next line */
	cur_x = round(cur_x + char_ht.x * cur_textstep);
	cur_y = round(cur_y + char_ht.y * cur_textstep);
	orig_x = cur_x;
	orig_y = cur_y;

	/* reset text size after any super/subscripting */
	work_fontsize = orig_fontsize;
	supersub = 0;

	is_newline = True;
	overlay_text_input(cur_x, cur_y);
}

static void
new_text_down(void)
{
	/* only so deep */
	if (supersub <= -MAX_SUPSUB)
		return;

	commit_current_text();
	turn_off_blinking_cursor();
	/* save current char height */
	heights[abs(supersub)] = char_ht;
	if (supersub-- > 0) {
		/* we were previously in a superscript, go back one */
		cur_x = round(cur_x + heights[supersub].x * PSUB_FRAC);
		cur_y = round(cur_y + heights[supersub].y * PSUB_FRAC);
		work_fontsize /= CSUB_FRAC;
	} else if (supersub < 0) {
		/* we were previously in a subscript, go deeper */
		cur_x = round(cur_x + char_ht.x * PSUB_FRAC);
		cur_y = round(cur_y + char_ht.y * PSUB_FRAC);
		work_fontsize *= CSUB_FRAC;
	}
	is_newline = False;
	overlay_text_input(cur_x, cur_y);
}

static void
new_text_up(void)
{
	/* only so deep */
	if (supersub >= MAX_SUPSUB)
		return;

	commit_current_text();
	turn_off_blinking_cursor();

	/* save current char height */
	heights[abs(supersub)] = char_ht;
	if (supersub++ < 0) {
		/* we were previously in a subscript, go back one */
		cur_x = round(cur_x - heights[-supersub].x * PSUB_FRAC);
		cur_y = round(cur_y - heights[-supersub].y * PSUB_FRAC);
		work_fontsize /= CSUB_FRAC;
	} else if (supersub > 0) {
		/* we were previously in a superscript, go deeper */
		cur_x = round(cur_x - char_ht.x * PSUB_FRAC);
		cur_y = round(cur_y - char_ht.y * PSUB_FRAC);
		work_fontsize *= CSUB_FRAC;
	}
	is_newline = False;
	overlay_text_input(cur_x, cur_y);
}

static void
set_ascentdescent(int asc, int desc, F_pos offset, double angle)
{
	/* quick code for horizontal text */
	if ((offset.y == 0  && offset.x != 0) ||
			(offset.x == 0 && offset.y == 0 && angle == 0.0)) {
		ascent.x = descent.x = char_ht.x = 0;
		char_ht.y = asc + desc;
		ascent.y = -asc;
		descent.y = desc;
	} else {
		double		sin_t, cos_t;

		if (offset.x == 0 && offset.y == 0) {
			sin_t = sin(angle);
			cos_t = cos(angle);
		} else {
			cos_t = sqrt(offset.x * offset.x + offset.y * offset.y);
			sin_t = -offset.y / cos_t;
			cos_t = offset.x / cos_t;
		}

		char_ht.x = lround((asc + desc) * sin_t);
		char_ht.y = lround((asc + desc) * cos_t);
		ascent.x = lround(-asc * sin_t);
		ascent.y = lround(-asc * cos_t);
		descent.x = lround(desc * sin_t);
		descent.y = lround(desc * cos_t);
	}
}

/* Version of init_text_input that allows overlaying.
 * Does not test for other text nearby.
 */

static void
overlay_text_input(int x, int y)
{
	int	asc;
	int	desc;

	cur_x = x;
	cur_y = y;

	set_action_on();
	set_mousefun("new text", "finish text", "cancel", "", "paste text", "");
	draw_mousefun_kbd();
	draw_mousefun_canvas();
	canvas_kbd_proc = (void (*)())char_handler;
	canvas_middlebut_proc = finish_text_input;
	canvas_leftbut_proc = finish_n_start;
	canvas_rightbut_proc = cancel_text_input;

	/*
	 * set working font info to current settings. This allows user to change
	 * font settings while we are in the middle of accepting text without
	 * affecting this text i.e. we don't allow the text to change midway
	 * through
	 */

	base_x = cur_x;
	base_y = cur_y;
	save_base_x = base_x;
	save_base_y = base_y;

	if (is_newline) {	/* working settings already set */
		is_newline = False;
	} else {		/* set working settings from ind panel */
		work_textcolor = cur_pencolor;
		work_font     = using_ps ? cur_ps_font : cur_latex_font;
		work_psflag   = using_ps;
		work_flags    = cur_textflags;
		work_textjust = cur_textjust;
		work_depth    = cur_depth;
		work_angle    = cur_elltextangle * M_PI / 180.0;
		while (work_angle < 0.0)
			work_angle += M_2PI;

		canvas_zoomed_xffont = font_open(work_psflag, work_font,
				work_fontsize * display_zoomscale, work_angle);
	}

	/* add new text */
	cur_t = new_text(1, "");
	start_suffix = 0;

	/* set the dimensions for the cursor and line spacing */
	font_ascentdescent(work_psflag, work_font, work_fontsize, &asc, &desc);
	set_ascentdescent(asc, desc, cur_t->offset, work_angle);

	put_msg("Ready for text input (from keyboard)");
	initialize_char_handler();
}

static void
init_text_input(int x, int y)
{
	int	asc;
	int	desc;

	cur_x = x;
	cur_y = y;

	/* clear canvas loc move proc in case we were in text select mode */
	canvas_locmove_proc = null_proc;

	set_action_on();
	set_mousefun("new text", "finish text", "cancel", "", "paste text", "");
	draw_mousefun_kbd();
	draw_mousefun_canvas();
	canvas_kbd_proc = (void (*)())char_handler;
	canvas_middlebut_proc = finish_text_input;
	canvas_leftbut_proc = finish_n_start;
	canvas_rightbut_proc = cancel_text_input;

	/*
	 * set working font info to current settings. This allows user to change
	 * font settings while we are in the middle of accepting text without
	 * affecting this text i.e. we don't allow the text to change midway
	 * through
	 */

	if ((old_t = text_search(cur_x, cur_y)) == NULL) {

		/******************/
		/* new text input */
		/******************/

		/* set origin where mouse was clicked */
		base_x = orig_x = x;
		base_y = orig_y = y;
		save_base_x = base_x;
		save_base_y = base_y;

		/* set working settings from ind panel */
		if (is_newline) {
			/* working settings already set from previous text */
			is_newline = False;
		} else {
			/* set working settings from ind panel */
			work_textcolor = cur_pencolor;
			work_fontsize = orig_fontsize = cur_fontsize;
			work_font     = using_ps ? cur_ps_font : cur_latex_font;
			work_psflag   = using_ps;
			work_flags    = cur_textflags;
			work_textjust = cur_textjust;
			work_depth    = cur_depth;
			work_angle    = cur_elltextangle*M_PI/180.0;
			while (work_angle < 0.0)
				work_angle += M_2PI;

			/* get the font for actually drawing on the canvas */
			canvas_zoomed_xffont = font_open(work_psflag, work_font,
					work_fontsize * display_zoomscale,
					work_angle);
		} /* (is_newline) */

		cur_t = new_text(1, "");
		start_suffix = 0;

	} else {

		/*****************/
		/* existing text */
		/*****************/

		if (hidden_text(old_t)) {
			put_msg("Can't edit hidden text");
			text_drawing_selected();
			return;
		}

		cur_t = copy_text(old_t);
		/* make old_t "invisible" for redrawing */
		first_char_old_t = old_t->cstring[0];
		old_t->cstring[0] = '\0';

		/* update the working text parameters */
		work_textcolor = cur_t->color;
		work_font = cur_t->font;
		canvas_zoomed_xffont = cur_t->xf_font;
		work_fontsize = cur_t->size;
		work_psflag   = cur_t->flags & PSFONT_TEXT;
		work_flags    = cur_t->flags;
		work_textjust = cur_t->type;
		work_depth    = cur_t->depth;
		work_angle    = cur_t->angle;
		while (work_angle < 0.0)
			work_angle += M_2PI;

		toggle_textmarker(cur_t);
		base_x = cur_t->base_x;
		base_y = cur_t->base_y;
		save_base_x = base_x;
		save_base_y = base_y;

		/* set origin to base of this text so newline will go there */
		orig_x = base_x;
		orig_y = base_y;
		orig_fontsize = work_fontsize;

		/* adjust the drawing origin, depending on the text alignment */
		text_origin(&base_x, &base_y, base_x, base_y, cur_t->type,
				cur_t->offset);

		if (split_at_cursor(cur_t, base_x, base_y, &cur_x, &cur_y,
					&start_suffix))
			/* invalid text */
			return;
	}

	/* reset super/subscript counter */
	supersub = 0;

	put_msg("Ready for text input (from keyboard)");

	refresh_character_panel(work_psflag, work_font);

	/* set the dimensions for the cursor and line spacing */
	font_ascentdescent(work_psflag, work_font, work_fontsize, &asc, &desc);
	set_ascentdescent(asc, desc, cur_t->offset, work_angle);

	/* save original char_ht for newline */
	orig_ht = char_ht;
	initialize_char_handler();
}

static F_text *
new_text(int len, char *string)
{
	F_text	   *text;

	if ((text = create_text()) == NULL)
		return (NULL);

	if ((text->cstring = new_string(len)) == NULL) {
		free(text);
		return (NULL);
	}
	text->type = work_textjust;
	text->font = work_font;	/* put in current font number */
	text->xf_font = canvas_zoomed_xffont;
	text->zoom = zoomscale;
	text->size = work_fontsize;
	text->angle = work_angle;
	text->flags = work_flags;
	text->color = cur_pencolor;
	text->depth = work_depth;
	text->pen_style = -1;
	text->base_x = base_x;
	text->base_y = base_y;
	strcpy(text->cstring, string);
	font_setbbox_offset(text);
	text->next = NULL;
	return (text);
}


/*
 * Return the cursor position (pixels into the string)
 * and the index of the character after the cursor.
 */
static int
split_at_cursor(F_text *t, int base_x, int base_y,
		int *x, int *y, int *start_suffix)
{
	int		dum;
	int		pos;   /* number of chars, then index of start_suffix */
	long		len2;
	long		denom2;
	long		p_dot_b;
	size_t		cstring_len;
	F_pos		b;
	F_pos		p;
	F_pos		right = {0, 0};
	F_pos		left;

	cstring_len = strlen(t->cstring);

	/* get the number of codepoints (pos) in t->cstring */
	if (!XftUtf8Len((XftChar8 *)t->cstring, (int)cstring_len, &pos, &dum)) {
		XftChar32	u;
		pos = 0;
		/* XftUtf8ToUcs4() returns the bytes read from src */
		while ((dum = XftUtf8ToUcs4((XftChar8*)(t->cstring + pos), &u,
						(int)(cstring_len - pos))) > 0)
			pos += dum;
		put_msg("Invalid utf8-sequence after: %.*s", pos, t->cstring);
		return -2;
	}

	/*
	 * Compute the distance of the cursor from the text origin
	 *
	 *	      X p = (x - base_x, y - base_y)
	 *	     /
         *	    /    projection (length): len = skalar(vec p, vec (unit b)),
	 *	   /     projection (vector): len * vec (unit b)
	 *	  /      hence: proj_x = (px*bx + py*by) * bx / (bx*bx + by*by)
	 *	 /
	 *	+-----|-------------> b = (t->offset.x, t->offset.y)
	 *	  len
	 *
	 * Also, continue to work in screen pixels (* zoomscale).
	 */

	b.x = t->offset.x * zoomscale + 0.5;
	b.y = t->offset.y * zoomscale + 0.5;
	denom2 = (long)b.x * b.x + (long)b.y * b.y;
	p_dot_b = (long)((*x - base_x) * zoomscale + 0.5) * b.x +
		(long)((*y - base_y) * zoomscale + 0.5) * b.y;

	/* round halfway cases up toward positive infinity */
	/* p_dot_b * b.x easily might overflow an int, hence use long;
	   if long overflows - well, then the cursor is at the wrong position */
	p.x = (p_dot_b * b.x + denom2 / 2) / denom2;
	p.y = (p_dot_b * b.y + denom2 / 2) / denom2;
	len2 = p.x * p.x + p.y * p.y;

	/* estimate the index of the character under the cursor */
	/* this could overflow, short**3 could be larger than unsigned */
	if (ULONG_MAX / len2 > cstring_len * cstring_len)
		pos = sqrt((len2 * cstring_len * cstring_len) / denom2);
	else
		pos = sqrt(((double)len2 * cstring_len * cstring_len) / denom2);
	if ((unsigned)pos > cstring_len - 1)
		pos = cstring_len - 1;

	/* walk left until the text is shorter than the current position */
	while(
			/* move to the first byte of a valid utf-8 sequence */
			begin_utf8char((unsigned char *)t->cstring, &pos),
			left = font_textlen(t->xf_font, t->font, t->cstring, pos),
			(long)left.x * left.x + (long)left.y * left.y > len2
	     ) {
		right = left;
		--pos;
	}
	if (right.x == 0 && right.y == 0)
		while(
			end_utf8char((unsigned char *)t->cstring, &pos),
			right = font_textlen(t->xf_font, t->font, t->cstring, pos + 1),
			(long)right.x * right.x + (long)right.y * right.y <
					len2
		     ) {
			left = right;
			++pos;
		}
	/* Now pos points either to the begin or the end of the utf-8 sequence
	   below the cursor. */
	if (sqrt(len2) - sqrt((long)left.x * left.x + (long)left.y * left.y) <=
			sqrt((long)right.x * right.x + (long)right.y * right.y)
				- sqrt(len2)) {
		/* position the cursor at the left edge of the glyph */
		begin_utf8char((unsigned char *)t->cstring, &pos);
		*start_suffix = pos;
		p = left;
	} else {
		end_utf8char((unsigned char *)t->cstring, &pos);
		*start_suffix = pos + 1;
		p = right;
	}
	/* back to fig units */
	*x = p.x / zoomscale + base_x + 0.5;
	*y = p.y / zoomscale + base_y + 0.5;

	return 0;
}


/* Move *pos to the index of the first byte of an utf8 char. */
void
begin_utf8char(unsigned char *str, int *pos)
{
	/* Skip over combining diacritical marks; These are in the range U+0300
	   to U+036F, which corresponds to UTF-8 0xcc 0x80 to 0xcd 0xaf. */
	if (*pos > 2 && ((str[*pos-1] == 0xcc && str[*pos] > 0x7f) ||
				(str[*pos-1] == 0xcd && str[*pos] < 0xb0)))
		*pos -= 2;
	while (*pos > 0 && str[*pos] > 0x7f && str[*pos] < 0xc2)
		--*pos;
}

/* Move *pos to the index of the last byte of an utf8 char. */
void
end_utf8char(unsigned char *str, int *pos)
{
	/* an ascii char, or end of string */
	if (str[*pos] < 0x80)
		return;

	/* the first byte tells the length of the utf8 byte sequence */
	if (str[*pos] > 0xfc)
		*pos += 5;
	else if (str[*pos] > 0xf8)
		*pos += 4;
	else if (str[*pos] > 0xf0)
		*pos += 3;
	else if (str[*pos] > 0xe0)
		*pos += 2;
	else if (str[*pos] > 0xc0)
		*pos += 1;

	/* Include combining diacritical marks; These are in the range U+0300
	   to U+036F, which corresponds to UTF-8 0xcc 0x80 to 0xcd 0xaf. */
	if (str[*pos+1] && str[*pos+2] &&
			((str[*pos+1] == 0xcc && str[*pos+2] > 0x7f) ||
			 (str[*pos+1] == 0xcd && str[*pos+2] < 0xb0)))
		*pos += 2;
}


/*******************************************************************

	char handling routines

*******************************************************************/

#define			BLINK_INTERVAL	700	/* milliseconds blink rate */

static void
draw_cursor(int x, int y)
{
	pw_vector(canvas_win, x + descent.x, y + descent.y,
			x + ascent.x, y + ascent.y,
			INV_PAINT, 1, RUBBER_LINE, 0.0, DEFAULT);
}

static void (*erase_cursor)(int x, int y) = draw_cursor;

static void
initialize_char_handler(void)
{
	turn_on_blinking_cursor(cur_x, cur_y);
	if (xim_ic != NULL) {
		put_msg("Ready for text input "
				"(from keyboard with input-method)");
		XSetICFocus(xim_ic);
		xim_active = True;
		xim_set_spot(cur_x, cur_y);
	}
}

static void
terminate_char_handler(void)
{
	turn_off_blinking_cursor();
	if (xim_ic != NULL) XUnsetICFocus(xim_ic);
	xim_active = False;
}

void
char_handler(unsigned char *c, int clen, KeySym keysym)
{
	int    i;

	if (clen == 1 && c[0] == ESC) {
		cancel_text_input();
	} else if (clen == 1 && (c[0] == CR || c[0] == NL)) {
		new_text_line();
	} else if (clen == 1 && c[0] == CTRL_UNDERSCORE) {
		/* subscript */
		new_text_down();
	} else if (clen == 1 && c[0] == CTRL_HAT) {
		/* superscript */
		new_text_up();

		/******************************************************/
		/* move cursor left - move char from prefix to suffix */
		/* Control-B and the Left arrow key both do this      */
		/******************************************************/
	} else if (keysym == XK_Left || (clen == 1 && c[0] == CTRL_B)) {
		/* already at the beginning of the string, return */
		if (start_suffix == 0)
			return;
		--start_suffix;
		begin_utf8char((unsigned char *)cur_t->cstring, &start_suffix);
		text_origin(&cur_x, &cur_y, cur_t->base_x, cur_t->base_y,
				cur_t->type, cur_t->offset);
		if (start_suffix > 0) {
			F_pos	off = font_textoffset(cur_t->xf_font,
						cur_t->font, cur_t->cstring,
						start_suffix);
			cur_x += off.x;
			cur_y += off.y;
		}
		move_blinking_cursor(cur_x, cur_y);

		/*******************************************************/
		/* move cursor right - move char from suffix to prefix */
		/* Control-F and Right arrow key both do this          */
		/*******************************************************/
	} else if (keysym == XK_Right || (clen == 1 && c[0] == CTRL_F)) {
		/* already at the end of the string, return */
		if (cur_t->cstring[start_suffix] == '\0')
			return;
		end_utf8char((unsigned char *)cur_t->cstring, &start_suffix);
		++start_suffix;
		text_origin(&cur_x, &cur_y, cur_t->base_x, cur_t->base_y,
				cur_t->type, cur_t->offset);
		if (cur_t->cstring[start_suffix] == '\0') {
			cur_x += cur_t->offset.x;
			cur_y += cur_t->offset.y;
		} else {
			F_pos	off = font_textoffset(cur_t->xf_font,
						cur_t->font, cur_t->cstring,
						start_suffix);
			cur_x += off.x;
			cur_y += off.y;
		}
		move_blinking_cursor(cur_x, cur_y);

		/**************************************************************/
		/* move cursor to beginning of text - put everything in suffix*/
		/* Control-A and Home key both do this                        */
		/**************************************************************/
	} else if (keysym == XK_Home || (clen == 1 && c[0] == CTRL_A)) {
		if (start_suffix == 0)
			return;
		else
			start_suffix = 0;

		text_origin(&cur_x, &cur_y, cur_t->base_x, cur_t->base_y,
				cur_t->type, cur_t->offset);
		move_blinking_cursor(cur_x, cur_y);

		/*********************************************************/
		/* move cursor to end of text - put everything in prefix */
		/* Control-E and End key both do this                    */
		/*********************************************************/
	} else if (keysym == XK_End || (clen == 1 && c[0] == CTRL_E)) {
		size_t	len = strlen(cur_t->cstring);

		if (start_suffix == (int)len)
			return;
		else
			start_suffix = (int)len;

		text_origin(&cur_x, &cur_y, cur_t->base_x, cur_t->base_y,
				cur_t->type, cur_t->offset);
		cur_x += cur_t->offset.x;
		cur_y += cur_t->offset.y;
		move_blinking_cursor(cur_x, cur_y);

		/******************************************/
		/* backspace - delete char left of cursor */
		/******************************************/
	} else if (clen == 1 && c[0] == CTRL_H) {
		size_t	len;
		int	o;
		int	xmin, xmax, ymin, ymax;

		if (start_suffix == 0)
			return;

		len = strlen(cur_t->cstring);

		/* compute the amount to move the suffix to the left */
		o = start_suffix - 1;
		begin_utf8char((unsigned char *)cur_t->cstring, &o);
		o = start_suffix - o;

		/* memmove */
		for (i = start_suffix; i <= (int)len; ++i)
			cur_t->cstring[i - o] = cur_t->cstring[i];
		start_suffix -= o;

		/*
		 * The cursor is drawn by inverse painting.
		 * This could interfere with re-drawing the text.
		 * Therefore, temporarily turn off the cursor.
		 */
		turn_off_blinking_cursor();

		/* erase the area of the original text */
		text_bound(cur_t, &xmin, &ymin, &xmax, &ymax);
		erase_box(xmin, ymin, xmax, ymax);

		font_setbbox_offset(cur_t);
		text_origin(&cur_x, &cur_y, cur_t->base_x, cur_t->base_y,
				cur_t->type, cur_t->offset);

		/* get the new cursor position */
		if (start_suffix == (int)(len - o)) {
			cur_x += cur_t->offset.x;
			cur_y += cur_t->offset.y;
		} else if (start_suffix > 0) {
			F_pos	off = font_textoffset(cur_t->xf_font,
						cur_t->font, cur_t->cstring,
						start_suffix);
			cur_x += off.x;
			cur_y += off.y;
		}

		/* redraw the area that was erased above */
		redisplay_zoomed_region(xmin, ymin, xmax, ymax);
		turn_on_blinking_cursor(cur_x, cur_y);

		/*****************************************/
		/* delete char to right of cursor        */
		/* Control-D and Delete key both do this */
		/*****************************************/
	} else if (clen == 1 && (c[0] == DEL || c[0] == CTRL_D)) {
		size_t	len = strlen(cur_t->cstring);
		int	o;
		int	xmin, xmax, ymin, ymax;

		if (start_suffix == (int)len)
			return;

		/* compute the amount to move the suffix to the right */
		o = start_suffix;
		end_utf8char((unsigned char *)cur_t->cstring, &o);
		o = o + 1 - start_suffix;

		for (i = start_suffix; i <= (int)(len - o); ++i)
			cur_t->cstring[i] = cur_t->cstring[i + o];

		turn_off_blinking_cursor();

		/* erase the area of the original text */
		text_bound(cur_t, &xmin, &ymin, &xmax, &ymax);
		erase_box(xmin, ymin, xmax, ymax);

		font_setbbox_offset(cur_t);
		text_origin(&cur_x, &cur_y, cur_t->base_x, cur_t->base_y,
				cur_t->type, cur_t->offset);
		if (start_suffix == (int)(len - o)) {
			cur_x += cur_t->offset.x;
			cur_y += cur_t->offset.y;
		} else if (start_suffix > 0) /* && start_suffix < len - o */ {
			F_pos	off = font_textoffset(cur_t->xf_font,
						cur_t->font, cur_t->cstring,
						start_suffix);
			cur_x += off.x;
			cur_y += off.y;
		}

		/* redraw the area that was erased above */
		redisplay_zoomed_region(xmin, ymin, xmax, ymax);
		turn_on_blinking_cursor(cur_x, cur_y);

		/*******************************/
		/* delete to beginning of line */
		/*******************************/
	} else if (clen == 1 && c[0] == CTRL_X) {
		size_t	len;
		int	xmin, xmax, ymin, ymax;

		if (start_suffix == 0)
			return;

		len = strlen(cur_t->cstring);

		/* move to the left by start_suffix */
		for (i = start_suffix; i <= (int)len; ++i)
			cur_t->cstring[i - start_suffix] = cur_t->cstring[i];
		start_suffix = 0;

		turn_off_blinking_cursor();

		/* erase the area of the original text */
		text_bound(cur_t, &xmin, &ymin, &xmax, &ymax);
		erase_box(xmin, ymin, xmax, ymax);

		font_setbbox_offset(cur_t);
		/* get the new cursor position */
		text_origin(&cur_x, &cur_y, cur_t->base_x, cur_t->base_y,
				cur_t->type, cur_t->offset);

		/* redraw the area that was erased above */
		redisplay_zoomed_region(xmin, ymin, xmax, ymax);
		turn_on_blinking_cursor(cur_x, cur_y);

		/*************************/
		/* delete to end of line */
		/*************************/
	} else if (clen == 1 && c[0] == CTRL_K) {
		size_t	len = strlen(cur_t->cstring);
		int	xmin, xmax, ymin, ymax;

		if (start_suffix == (int)len)
			return;

		cur_t->cstring[start_suffix] = '\0';

		turn_off_blinking_cursor();

		/* erase the area of the original text */
		text_bound(cur_t, &xmin, &ymin, &xmax, &ymax);
		erase_box(xmin, ymin, xmax, ymax);

		font_setbbox_offset(cur_t);

		/* get the new cursor position */
		text_origin(&cur_x, &cur_y, cur_t->base_x, cur_t->base_y,
				cur_t->type, cur_t->offset);
		cur_x += cur_t->offset.x;
		cur_y += cur_t->offset.y;

		/* redraw the area that was erased above */
		redisplay_zoomed_region(xmin, ymin, xmax, ymax);
		turn_on_blinking_cursor(cur_x, cur_y);

	} else if (clen == 1 && c[0] < SP) {
		put_msg("Invalid character ignored");

		/*************************/
		/* normal text character */
		/*************************/
	} else {
		size_t	len = strlen(cur_t->cstring);
		F_pos	off;

		turn_off_blinking_cursor();
		cur_t->cstring = realloc(cur_t->cstring,
						len + (size_t)(clen + 1));

		cur_t->cstring[len + clen] = '\0';
		for (i = len - 1; i >= start_suffix; --i)
			cur_t->cstring[i + clen] = cur_t->cstring[i];
		memcpy(cur_t->cstring + start_suffix, c, clen);
		start_suffix += clen;
		font_setbbox_offset(cur_t);
		redisplay_text(cur_t);

		/* determine the cursor position */
		/* first, assign the current text origin to  cur_x, cur_y */
		text_origin(&cur_x, &cur_y, cur_t->base_x, cur_t->base_y,
					cur_t->type, cur_t->offset);
		off = font_textoffset(cur_t->xf_font, cur_t->font,
					cur_t->cstring, start_suffix);
		cur_x += off.x;
		cur_y += off.y;
		/* free_text would also free comments, fonts,
		   and follow t->next */
		turn_on_blinking_cursor(cur_x, cur_y);
	}
}


/****************************************************************/
/*								*/
/*		Blinking cursor handling routines		*/
/*								*/
/****************************************************************/

static int	cursor_on, cursor_is_moving;
static int	cursor_x, cursor_y;
static XtTimerCallbackProc blink(XtPointer client_data, XtIntervalId *id);
static int	stop_blinking = False;
static int	cur_is_blinking = False;

static void
turn_on_blinking_cursor(int x, int y)
{
	unsigned long	blink_timer = BLINK_INTERVAL;
	cursor_is_moving = 0;
	cursor_x = x;
	cursor_y = y;
	draw_cursor(x, y);
	cursor_on = 1;
	if (!cur_is_blinking) {	/* if we are already blinking, don't request
				 * another */
		(void) XtAppAddTimeOut(tool_app, blink_timer,
					(XtTimerCallbackProc)blink,
					(XtPointer)blink_timer);
		cur_is_blinking = True;
	}
	stop_blinking = False;
}

static void
turn_off_blinking_cursor(void)
{
	if (cursor_on)
		erase_cursor(cursor_x, cursor_y);
	stop_blinking = True;
}

static	XtTimerCallbackProc
blink(XtPointer client_data, XtIntervalId *id)
{
	(void)id;
	union {
		XtPointer	ptr;
		unsigned long	value;
	} client = {client_data};

	if (!stop_blinking) {
		if (cursor_is_moving)
			return (0);
		if (cursor_on) {
			erase_cursor(cursor_x, cursor_y);
			cursor_on = 0;
		} else {
			draw_cursor(cursor_x, cursor_y);
			cursor_on = 1;
		}
		(void) XtAppAddTimeOut(tool_app, client.value,
					(XtTimerCallbackProc) blink,
					(XtPointer)client.ptr);
	} else {
		stop_blinking = False;	/* signal that we've stopped */
		cur_is_blinking = False;
	}
	return (0);
}

static void
move_blinking_cursor(int x, int y)
{
	cursor_is_moving = 1;
	if (cursor_on)
		erase_cursor(cursor_x, cursor_y);
	cursor_x = x;
	cursor_y = y;
	draw_cursor(cursor_x, cursor_y);
	cursor_on = 1;
	cursor_is_moving = 0;
	if (xim_active) xim_set_spot(x, y);
}


/****************************************************************/
/*								*/
/*		Internationalization utility procedures		*/
/*								*/
/****************************************************************/

static void
GetPreferredGeomerty(XIC ic, char *name, XRectangle **area)
{
	XVaNestedList list;
	list = XVaCreateNestedList(0, XNAreaNeeded, area, NULL);
	XGetICValues(ic, name, list, NULL);
	XFree(list);
}

static void
SetGeometry(XIC ic, char *name, XRectangle *area)
{
	XVaNestedList list;
	list = XVaCreateNestedList(0, XNArea, area, NULL);
	XSetICValues(ic, name, list, NULL);
	XFree(list);
}

void
xim_set_ic_geometry(XIC ic, int width, int height)
{
	XRectangle preedit_area, *preedit_area_ptr;
	XRectangle status_area, *status_area_ptr;

	if (xim_ic == NULL) return;

	if (appres.DEBUG)
		fprintf(stderr, "xim_set_ic_geometry(%d, %d)\n", width, height);

	if (xim_style & XIMStatusArea) {
		GetPreferredGeomerty(ic, XNStatusAttributes, &status_area_ptr);
		status_area.width = status_area_ptr->width;
		if (width / 2 < status_area.width)
			status_area.width = width / 2;
		status_area.height = status_area_ptr->height;
		status_area.x = 0;
		status_area.y = height - status_area.height;
		SetGeometry(xim_ic, XNStatusAttributes, &status_area);
		if (appres.DEBUG)
			fprintf(stderr, "status geometry: %dx%d+%d+%d\n",
				status_area.width, status_area.height,
				status_area.x, status_area.y);
	}
	if (xim_style & XIMPreeditArea) {
		GetPreferredGeomerty(ic, XNPreeditAttributes,
							&preedit_area_ptr);
		preedit_area.width = preedit_area_ptr->width;
		if (preedit_area.width < width - status_area.width)
			preedit_area.width = width - status_area.width;
		if (width < preedit_area.width)
			preedit_area.width = width;
		preedit_area.height = preedit_area_ptr->height;
		preedit_area.x = width - preedit_area.width;
		preedit_area.y = height - preedit_area.height;
		SetGeometry(xim_ic, XNPreeditAttributes, &preedit_area);
		if (appres.DEBUG)
			fprintf(stderr, "preedit geometry: %dx%d+%d+%d\n",
				preedit_area.width, preedit_area.height,
				preedit_area.x, preedit_area.y);
	}
}

Boolean
xim_initialize(Widget w)
{
	const XIMStyle	style_notuseful = 0;
	const XIMStyle	style_over_the_spot =
					XIMPreeditPosition | XIMStatusArea;
	const XIMStyle	style_old_over_the_spot =
					XIMPreeditPosition | XIMStatusNothing;
	const XIMStyle	style_off_the_spot = XIMPreeditArea | XIMStatusArea;
	const XIMStyle	style_root = XIMPreeditNothing | XIMStatusNothing;
	const XIMStyle	style_none = XIMPreeditNone | XIMStatusNone;
	XIMStyles	*styles;
	XIMStyle	preferred_style;
	int		i;
	XVaNestedList	preedit_att, status_att;
	XPoint		spot;
	char		*modifier_list;

	preferred_style = style_notuseful;
	if (strncasecmp(appres.xim_input_style, "OverTheSpot", 3) == 0)
		preferred_style = style_over_the_spot;
	else if (strncasecmp(appres.xim_input_style, "OldOverTheSpot", 6) == 0)
		preferred_style = style_old_over_the_spot;
	else if (strncasecmp(appres.xim_input_style, "OffTheSpot", 3) == 0)
		preferred_style = style_off_the_spot;
	else if (strncasecmp(appres.xim_input_style, "Root", 3) == 0)
		preferred_style = style_root;
	else if (strncasecmp(appres.xim_input_style, "None", 3) != 0)
		fprintf(stderr, "xfig: inputStyle should be OverTheSpot, "
				"OffTheSpot, or Root\n");

	if (preferred_style == style_notuseful)
		return False;

	if (appres.DEBUG) fprintf(stderr, "initialize_input_method()...\n");
	if ((modifier_list = XSetLocaleModifiers("@im=none")) == NULL ||
			*modifier_list == '\0') {
		printf("Warning: XSetLocaleModifiers() failed.\n");
	} else

		xim_im = XOpenIM(XtDisplay(w), NULL, NULL, NULL);
	if (xim_im == NULL) {
		fprintf(stderr, "xfig: can't open input-method\n");
		return False;
	}
	XGetIMValues(xim_im, XNQueryInputStyle, &styles, NULL, NULL);
	for (i = 0; i < styles->count_styles; i++) {
		if (appres.DEBUG)
			fprintf(stderr, "styles[%d]=%lx\n",
					i, styles->supported_styles[i]);
		if (styles->supported_styles[i] == preferred_style) {
			xim_style = preferred_style;
		} else if (styles->supported_styles[i] == style_root) {
			if (xim_style == 0) xim_style = style_root;
		} else if (styles->supported_styles[i] == style_none) {
			if (xim_style == 0) xim_style = style_none;
		}
	}
	if (xim_style != preferred_style && *modifier_list != '\0' &&
			!strstr(modifier_list,"@im=local") &&
			!strstr(modifier_list,"@im=none")) {
		fprintf(stderr, "xfig: this input-method does not support "
				"%s input style\n", appres.xim_input_style);
		if (xim_style == 0) {
			fprintf(stderr, "xfig: it does not support ROOT "
					"input style, too...\n");
			return False;
		} else {
			fprintf(stderr, "xfig: using ROOT or NONE input "
					"style instead.\n");
		}
	}
	if (appres.DEBUG) {
		char *s;
		if (xim_style == style_over_the_spot) s = "OverTheSpot";
		else if (xim_style == style_off_the_spot) s = "OffTheSpot";
		else if (xim_style == style_root) s = "Root";
		else if (xim_style == style_none) s = "None";
		else s = "unknown";
		fprintf(stderr, "xfig: selected input style: %s\n", s);
	}

	if (xim_style == style_none) {
		xim_ic = XCreateIC(xim_im, XNInputStyle, xim_style, NULL, NULL);
	} else {
		spot.x = 20;  /* dummy */
		spot.y = 20;
		preedit_att = XVaCreateNestedList(0, XNFontSet,
				appres.fixed_fontset, XNSpotLocation, &spot,
				NULL);
		status_att = XVaCreateNestedList(0, XNFontSet,
				appres.fixed_fontset, NULL);
		xim_ic = XCreateIC(xim_im, XNInputStyle , xim_style,
				XNClientWindow, XtWindow(w),
				XNFocusWindow, XtWindow(w),
				XNPreeditAttributes, preedit_att,
				XNStatusAttributes, status_att, NULL, NULL);
		XFree(preedit_att);
		XFree(status_att);
	}
	if (xim_ic == NULL) {
		fprintf(stderr, "xfig: can't create input-context\n");
		return False;
	}

	if (appres.DEBUG) fprintf(stderr, "input method initialized\n");

	return True;
}

static void
xim_set_spot(int x, int y)
{
	static XPoint	spot;
	XVaNestedList	preedit_att;
	int		x1, y1;
	if (xim_ic != NULL) {
		if (xim_style & XIMPreeditPosition) {
			if (appres.DEBUG)
				fprintf(stderr, "xim_set_spot(%d,%d)\n", x, y);
			preedit_att = XVaCreateNestedList(0, XNSpotLocation,
								&spot, NULL);
			x1 = ZOOMX(x) + 1;
			y1 = ZOOMY(y);
			if (x1 < 0) x1 = 0;
			if (y1 < 0) y1 = 0;
			spot.x = x1;
			spot.y = y1;
			XSetICValues(xim_ic, XNPreeditAttributes, preedit_att,
								NULL);
			XFree(preedit_att);
		}
	}
}

static Boolean
is_preedit_running(void)
{
	pid_t	pid;
	sprintf(preedit_filename, "%s/%s%06d",
					TMPDIR, "xfig-preedit", getpid());
	pid = waitpid(-1, NULL, WNOHANG);
	if (0 < preedit_pid && pid == preedit_pid)
		preedit_pid = -1;
	return (0 < preedit_pid && access(preedit_filename, R_OK) == 0);
}

void
kill_preedit(void)
{
	if (0 < preedit_pid) {
		kill(preedit_pid, SIGTERM);
		preedit_pid = -1;
	}
}

static void
close_preedit_proc(int x, int y)
{
	(void)x;
	(void)y;

	if (is_preedit_running()) {
		kill_preedit();
		put_msg("Pre-edit window closed");
	}
	text_drawing_selected();
	draw_mousefun_canvas();
}

static void
open_preedit_proc(int x, int y)
{
	(void)x;
	(void)y;

	int	i;
	if (!is_preedit_running()) {
		put_msg("Opening pre-edit window...");
		draw_mousefun_canvas();
		set_temp_cursor(wait_cursor);
		preedit_pid = fork();
		if (preedit_pid == -1) {  /* cannot fork */
			fprintf(stderr, "Cannot fork the process: %s\n",
					strerror(errno));
		} else if (preedit_pid == 0) {
			/* child process; execute xfig-preedit */
			execlp(appres.text_preedit, appres.text_preedit,
					preedit_filename, NULL);
			fprintf(stderr, "Cannot execute %s\n",
					appres.text_preedit);
			exit(-1);
		} else {  /* parent process; wait until xfig-preedit is up */
			for (i = 0; i < 10 && !is_preedit_running(); i++)
				sleep(1);
		}
		if (is_preedit_running())
			put_msg("Pre-edit window opened");
		else
			put_msg("Cannot open pre-edit window");
		reset_cursor();
	}
	text_drawing_selected();
	draw_mousefun_canvas();
}

static void
paste_preedit_proc(int x, int y)
{
	FILE	*fp;
	int	ch;
	if (!is_preedit_running()) {
		open_preedit_proc(x, y);
	} else if ((fp = fopen(preedit_filename, "r")) != NULL) {
		init_text_input(x, y);
		while ((ch = getc(fp)) != EOF) {
			if (ch == '\\')
				new_text_line();
			else
				prefix[leng_prefix++] = ch;
		}
		prefix[leng_prefix] = '\0';
		finish_text_input(0,0,0);
		fclose(fp);
		put_msg("Text pasted from pre-edit window");
	} else {
		put_msg("Cannot get text from pre-edit window");
	}
	text_drawing_selected();
	draw_mousefun_canvas();
}
