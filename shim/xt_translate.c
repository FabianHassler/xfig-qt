/*
 * xt_translate.c - a small implementation of Xt action tables and
 * translation tables.
 *
 * xfig binds events to its functions through translation tables such as
 *	"Any<BtnDown>:EventCanv()\n<Key>F18: PasteCanv()".
 * The null widgets keep these tables, and the Qt UI dispatches synthesized
 * X events through them with xq_dispatch_event(), so every panel that draws
 * into a window (canvas, rulers, layer buttons, ...) gets the event handling
 * of the original program.
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "xshim.h"
#include "xq_widget.h"

/* ---------------------------------------------------------------- actions */

typedef struct ActionEntry {
	char		*name;
	XtActionProc	proc;
	struct ActionEntry *next;
} ActionEntry;

static ActionEntry *actions = NULL;

void
XtAppAddActions(XtAppContext app, XtActionList list, Cardinal n)
{
	(void)app;
	for (Cardinal i = 0; i < n; ++i) {
		ActionEntry *a = malloc(sizeof *a);
		a->name = strdup(list[i].string);
		a->proc = list[i].proc;
		a->next = actions;	/* later additions take precedence */
		actions = a;
	}
}

static XtActionProc
find_action(const char *name)
{
	for (ActionEntry *a = actions; a; a = a->next)
		if (strcmp(a->name, name) == 0)
			return a->proc;
	return NULL;
}

int
xq_call_action(Widget w, const char *name, XEvent *ev, String *params, Cardinal n)
{
	XtActionProc p = find_action(name);
	if (!p)
		return 0;
	p(w, ev, params, &n);
	return 1;
}

/* ----------------------------------------------------------- translations */

#define MAXACT	4
#define MAXPAR	4

typedef struct {
	char	*name;
	String	params[MAXPAR];
	Cardinal nparams;
} TAction;

typedef struct Translation {
	int		type;		/* X event type, 0 = unsupported */
	unsigned	button;		/* 0 = any */
	unsigned	motion_mask;	/* required buttons for motion */
	KeySym		keysym;		/* 0 = any key */
	unsigned	mods_on;	/* modifiers that must be set */
	unsigned	mods_off;	/* modifiers that must be clear */
	Boolean		exact;		/* "!" or "None": exactly mods_on */
	int		count;		/* "(2)": a double click */
	TAction		act[MAXACT];
	int		nact;
	struct Translation *next;
} Translation;

struct _TranslationData {
	Translation	*first;
};

static const char *
skip_ws(const char *s)
{
	while (*s == ' ' || *s == '\t')
		++s;
	return s;
}

static unsigned
modifier_mask(const char *m)
{
	if (!strcasecmp(m, "Shift"))	return ShiftMask;
	if (!strcasecmp(m, "Ctrl") || !strcasecmp(m, "Control")) return ControlMask;
	if (!strcasecmp(m, "Meta") || !strcasecmp(m, "Alt") ||
			!strcasecmp(m, "Mod1")) return Mod1Mask;
	if (!strcasecmp(m, "Lock"))	return LockMask;
	if (!strcasecmp(m, "Button1"))	return Button1Mask;
	if (!strcasecmp(m, "Button2"))	return Button2Mask;
	if (!strcasecmp(m, "Button3"))	return Button3Mask;
	return 0;
}

static void
event_type(Translation *t, const char *e)
{
	if (!strcasecmp(e, "Motion") || !strcasecmp(e, "MotionNotify") ||
			!strcasecmp(e, "PtrMoved") || !strcasecmp(e, "MouseMoved")) {
		t->type = MotionNotify;
	} else if (!strncasecmp(e, "Btn", 3) && isdigit((unsigned char)e[3]) &&
			!strcasecmp(e + 4, "Motion")) {
		t->type = MotionNotify;
		t->motion_mask = Button1Mask << (e[3] - '1');
	} else if (!strcasecmp(e, "BtnMotion") || !strcasecmp(e, "ButtonMotion")) {
		t->type = MotionNotify;
		t->motion_mask = Button1Mask | Button2Mask | Button3Mask;
	} else if (!strcasecmp(e, "BtnDown") || !strcasecmp(e, "ButtonPress")) {
		t->type = ButtonPress;
	} else if (!strcasecmp(e, "BtnUp") || !strcasecmp(e, "ButtonRelease")) {
		t->type = ButtonRelease;
	} else if (!strncasecmp(e, "Btn", 3) && isdigit((unsigned char)e[3])) {
		t->button = e[3] - '0';
		t->type = !strcasecmp(e + 4, "Up") ? ButtonRelease : ButtonPress;
	} else if (!strcasecmp(e, "Key") || !strcasecmp(e, "KeyDown") ||
			!strcasecmp(e, "KeyPress")) {
		t->type = KeyPress;
	} else if (!strcasecmp(e, "KeyUp") || !strcasecmp(e, "KeyRelease")) {
		t->type = KeyRelease;
	} else if (!strcasecmp(e, "EnterWindow") || !strcasecmp(e, "Enter") ||
			!strcasecmp(e, "EnterNotify")) {
		t->type = EnterNotify;
	} else if (!strcasecmp(e, "LeaveWindow") || !strcasecmp(e, "Leave") ||
			!strcasecmp(e, "LeaveNotify")) {
		t->type = LeaveNotify;
	} else if (!strcasecmp(e, "Expose")) {
		t->type = Expose;
	} else if (!strcasecmp(e, "Message") || !strcasecmp(e, "ClientMessage")) {
		t->type = ClientMessage;
	} else if (!strcasecmp(e, "Configure") || !strcasecmp(e, "ConfigureNotify")) {
		t->type = ConfigureNotify;
	} else {
		t->type = 0;
	}
}

static void
parse_actions(Translation *t, const char *s)
{
	char name[64];
	while (*(s = skip_ws(s)) && t->nact < MAXACT) {
		int n = 0;
		while (*s && *s != '(' && !isspace((unsigned char)*s) && n < 63)
			name[n++] = *s++;
		name[n] = '\0';
		if (n == 0)
			break;
		TAction *a = &t->act[t->nact++];
		a->name = strdup(name);
		a->nparams = 0;
		s = skip_ws(s);
		if (*s == '(') {
			++s;
			while (*s && *s != ')') {
				char par[64];
				int k = 0;
				s = skip_ws(s);
				while (*s && *s != ',' && *s != ')' && k < 63)
					par[k++] = *s++;
				while (k > 0 && isspace((unsigned char)par[k - 1]))
					--k;
				par[k] = '\0';
				if (k > 0 && a->nparams < MAXPAR)
					a->params[a->nparams++] = strdup(par);
				if (*s == ',')
					++s;
			}
			if (*s == ')')
				++s;
		}
	}
}

/* parse one line: [modifiers]<Event>[detail]: actions */
static Translation *
parse_line(const char *line)
{
	Translation *t = calloc(1, sizeof *t);
	const char *s = skip_ws(line);
	char word[64];

	/* modifiers */
	while (*s && *s != '<') {
		int neg = 0, n = 0;
		s = skip_ws(s);
		if (*s == '!') { t->exact = True; ++s; continue; }
		if (*s == ':' ) { ++s; continue; }
		if (*s == '~') { neg = 1; ++s; }
		while (*s && *s != '<' && !isspace((unsigned char)*s) && *s != '~' && n < 63)
			word[n++] = *s++;
		word[n] = '\0';
		if (!n)
			continue;
		if (!strcasecmp(word, "Any"))
			continue;
		if (!strcasecmp(word, "None")) {
			t->exact = True;
			continue;
		}
		if (neg)
			t->mods_off |= modifier_mask(word);
		else
			t->mods_on |= modifier_mask(word);
	}
	/* the event; of a sequence "<A>,<B>" only the last one is used */
	for (;;) {
		int n = 0;
		if (*s != '<') {
			free(t);
			return NULL;
		}
		++s;
		while (*s && *s != '>' && n < 63)
			word[n++] = *s++;
		word[n] = '\0';
		if (*s == '>')
			++s;
		t->button = 0;
		t->motion_mask = 0;
		t->keysym = 0;
		t->count = 0;
		event_type(t, word);
		/* a repeat count, e.g. "(2)" */
		if (*s == '(') {
			t->count = atoi(s + 1);
			while (*s && *s != ')')
				++s;
			if (*s == ')')
				++s;
		}
		/* detail, e.g. a key name */
		n = 0;
		s = skip_ws(s);
		while (*s && *s != ':' && *s != ',' && !isspace((unsigned char)*s) && n < 63)
			word[n++] = *s++;
		word[n] = '\0';
		if (n && (t->type == KeyPress || t->type == KeyRelease)) {
			t->keysym = XStringToKeysym(word);
			if (!t->keysym)		/* unknown key: never matches */
				t->keysym = (KeySym)-1;
		}
		s = skip_ws(s);
		if (*s != ',')
			break;
		s = skip_ws(s + 1);
		/* modifiers of the next event in the sequence */
		while (*s && *s != '<')
			++s;
	}
	while (*s && *s != ':')
		++s;
	if (*s == ':')
		++s;
	parse_actions(t, s);
	if (!t->type || !t->nact) {
		free(t);
		return NULL;
	}
	return t;
}

XtTranslations
XtParseTranslationTable(const char *table)
{
	struct _TranslationData *td = calloc(1, sizeof *td);
	Translation **tail = &td->first;
	const char *s = table;
	if (!s)
		return td;
	/* skip a leading "#override" etc. */
	if (*s == '#') {
		while (*s && *s != '\n')
			++s;
	}
	while (*s) {
		const char *e = strchr(s, '\n');
		size_t len = e ? (size_t)(e - s) : strlen(s);
		char *line = strndup(s, len);
		Translation *t = parse_line(line);
		free(line);
		if (t) {
			*tail = t;
			tail = &t->next;
		}
		s += len;
		if (*s == '\n')
			++s;
	}
	return td;
}

XtAccelerators
XtParseAcceleratorTable(const char *table)
{
	return XtParseTranslationTable(table);
}

/* per-widget translations, kept apart from the widget record */
typedef struct WidgetTrans {
	Widget		w;
	Translation	*first;
	struct WidgetTrans *next;
} WidgetTrans;

static WidgetTrans *wtrans = NULL;

static WidgetTrans *
widget_trans(Widget w, Boolean create)
{
	for (WidgetTrans *t = wtrans; t; t = t->next)
		if (t->w == w)
			return t;
	if (!create)
		return NULL;
	WidgetTrans *t = calloc(1, sizeof *t);
	t->w = w;
	t->next = wtrans;
	wtrans = t;
	return t;
}

static Translation *
copy_list(Translation *src)
{
	Translation *head = NULL, **tail = &head;
	for (; src; src = src->next) {
		Translation *c = malloc(sizeof *c);
		*c = *src;
		c->next = NULL;
		*tail = c;
		tail = &c->next;
	}
	return head;
}

static void
add_translations(Widget w, XtTranslations tt, Boolean override)
{
	if (!w || !tt)
		return;
	WidgetTrans *wt = widget_trans(w, True);
	Translation *c = copy_list(tt->first);
	if (!c)
		return;
	if (override) {
		Translation *last = c;
		while (last->next)
			last = last->next;
		last->next = wt->first;
		wt->first = c;
	} else {
		Translation **tail = &wt->first;
		while (*tail)
			tail = &(*tail)->next;
		*tail = c;
	}
}

void XtOverrideTranslations(Widget w, XtTranslations t) { add_translations(w, t, True); }
void XtAugmentTranslations(Widget w, XtTranslations t) { add_translations(w, t, False); }

void
xq_set_translations(Widget w, XtTranslations t)
{
	WidgetTrans *wt = widget_trans(w, True);
	wt->first = t ? copy_list(t->first) : NULL;
}

/* the click count of the event being dispatched, set by the Qt UI */
int	xq_click_count = 1;

static Boolean
matches(Translation *t, XEvent *ev)
{
	unsigned state = 0;
	if (t->type != ev->type)
		return False;
	if (t->count > 1 && xq_click_count < t->count)
		return False;
	switch (ev->type) {
	case ButtonPress:
	case ButtonRelease:
		state = ev->xbutton.state;
		if (t->button && t->button != ev->xbutton.button)
			return False;
		break;
	case MotionNotify:
		state = ev->xmotion.state;
		if (t->motion_mask && !(state & t->motion_mask))
			return False;
		break;
	case KeyPress:
	case KeyRelease:
		state = ev->xkey.state;
		if (t->keysym) {
			KeySym ks = XLookupKeysym(&ev->xkey, 0);
			/* key names are case-insensitive for letters */
			if (ks != t->keysym && !(ks < 0x80 && t->keysym < 0x80 &&
					tolower((int)ks) == tolower((int)t->keysym)))
				return False;
		}
		break;
	case EnterNotify:
	case LeaveNotify:
		state = ev->xcrossing.state;
		break;
	default:
		return True;
	}
	unsigned mods = state & (ShiftMask | ControlMask | Mod1Mask | LockMask);
	if ((mods & t->mods_on) != t->mods_on)
		return False;
	if (mods & t->mods_off)
		return False;
	if (t->exact && (mods & ~LockMask) != (t->mods_on & ~LockMask))
		return False;
	return True;
}

int
xq_translation_matches(Widget w, XEvent *ev)
{
	WidgetTrans *wt = widget_trans(w, False);
	for (Translation *t = wt ? wt->first : NULL; t; t = t->next)
		if (matches(t, ev))
			return 1;
	return 0;
}

int
xq_dispatch_event(Widget w, XEvent *ev)
{
	WidgetTrans *wt = widget_trans(w, False);
	if (!wt)
		return 0;
	/* a multiple click prefers the entries made for it */
	for (int pass = xq_click_count > 1 ? 0 : 1; pass < 2; ++pass)
		for (Translation *t = wt->first; t; t = t->next) {
			if ((pass == 0) != (t->count > 1) || !matches(t, ev))
				continue;
			for (int i = 0; i < t->nact; ++i)
				xq_call_action(w, t->act[i].name, ev, t->act[i].params,
						t->act[i].nparams);
			return 1;
		}
	return 0;
}
