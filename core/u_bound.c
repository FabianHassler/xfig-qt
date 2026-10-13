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

#include "u_bound.h"

#include <limits.h>		/* INT_MIN, INT_MAX */
#include <math.h>
#include <stdlib.h>

#include "resources.h"
#include "object.h"
#include "mode.h"
#include "paintop.h"
#include "u_colors.h"
#include "u_draw.h"
#include "u_fonts.h"
#include "w_canvas.h"		/* round_coords() */
#include "w_drawprim.h"
#include "w_file.h"
#include "w_layers.h"
#include "xfig_math.h"


#define		Ninety_deg		M_PI_2
#define		One_eighty_deg		M_PI
#define		Two_seventy_deg		(M_PI + M_PI_2)
#define		Three_sixty_deg		(M_PI + M_PI)
#define		half(z1 ,z2)		((z1+z2)/2.0)
#define         MIN_MAX(X1, X2, Y1, Y2)  \
                *xmax = max2(*xmax, X1); \
		*xmin = min2(*xmin, X2); \
	        *ymax = max2(*ymax, Y1); \
	        *ymin = min2(*ymin, Y2)

static void	points_bound(F_point *points, int half_wd, int *xmin, int *ymin, int *xmax, int *ymax);
static void	general_spline_bound(F_spline *s, int *xmin, int *ymin, int *xmax, int *ymax);
static void	approx_spline_bound(F_spline *s, int *xmin, int *ymin, int *xmax, int *ymax);
static void arrow_bound(int objtype, F_line *obj, int *xmin, int *ymin, int *xmax, int *ymax);

void arc_bound(F_arc *arc, int *xmin, int *ymin, int *xmax, int *ymax)
{
    float	    alpha, beta;
    double	    dx, dy, radius;
    int		    bx, by, sx, sy;
    int		    half_wd;

    dx = arc->point[0].x - arc->center.x;
    dy = arc->center.y - arc->point[0].y;
    if (dx==0.0)
	alpha = (dy > 0? Ninety_deg: -Ninety_deg);
    else
	alpha = atan2(dy, dx);

    if (alpha < 0.0)
	alpha += Three_sixty_deg;

    radius = sqrt(dx * dx + dy * dy);

    dx = arc->point[2].x - arc->center.x;
    dy = arc->center.y - arc->point[2].y;
    if (dx==0.0)
	beta = (dy > 0? Ninety_deg: -Ninety_deg);
    else
	beta = atan2(dy, dx);

    if (beta < 0.0)
	beta += Three_sixty_deg;

    bx = max2(arc->point[0].x, arc->point[1].x);
    bx = max2(arc->point[2].x, bx);
    by = max2(arc->point[0].y, arc->point[1].y);
    by = max2(arc->point[2].y, by);
    sx = min2(arc->point[0].x, arc->point[1].x);
    sx = min2(arc->point[2].x, sx);
    sy = min2(arc->point[0].y, arc->point[1].y);
    sy = min2(arc->point[2].y, sy);

    if (arc->direction == 1) {	/* counter clockwise */
	if (alpha > beta) {
	    if (alpha <= 0 || 0 <= beta)
		bx = (int) (arc->center.x + radius + 1.0);
	    if (alpha <= Ninety_deg || Ninety_deg <= beta)
		sy = (int) (arc->center.y - radius - 1.0);
	    if (alpha <= One_eighty_deg || One_eighty_deg <= beta)
		sx = (int) (arc->center.x - radius - 1.0);
	    if (alpha <= Two_seventy_deg || Two_seventy_deg <= beta)
		by = (int) (arc->center.y + radius + 1.0);
	} else {
	    if (0 <= beta && alpha <= 0)
		bx = (int) (arc->center.x + radius + 1.0);
	    if (Ninety_deg <= beta && alpha <= Ninety_deg)
		sy = (int) (arc->center.y - radius - 1.0);
	    if (One_eighty_deg <= beta && alpha <= One_eighty_deg)
		sx = (int) (arc->center.x - radius - 1.0);
	    if (Two_seventy_deg <= beta && alpha <= Two_seventy_deg)
		by = (int) (arc->center.y + radius + 1.0);
	}
    } else {			/* clockwise	 */
	if (alpha > beta) {
	    if (beta <= 0 && 0 <= alpha)
		bx = (int) (arc->center.x + radius + 1.0);
	    if (beta <= Ninety_deg && Ninety_deg <= alpha)
		sy = (int) (arc->center.y - radius - 1.0);
	    if (beta <= One_eighty_deg && One_eighty_deg <= alpha)
		sx = (int) (arc->center.x - radius - 1.0);
	    if (beta <= Two_seventy_deg && Two_seventy_deg <= alpha)
		by = (int) (arc->center.y + radius + 1.0);
	} else {
	    if (0 <= alpha || beta <= 0)
		bx = (int) (arc->center.x + radius + 1.0);
	    if (Ninety_deg <= alpha || beta <= Ninety_deg)
		sy = (int) (arc->center.y - radius - 1.0);
	    if (One_eighty_deg <= alpha || beta <= One_eighty_deg)
		sx = (int) (arc->center.x - radius - 1.0);
	    if (Two_seventy_deg <= alpha || beta <= Two_seventy_deg)
		by = (int) (arc->center.y + radius + 1.0);
	}
    }

    /* If we have a pie wedge then the center must be considered
     * when calc. the bounding box.
     */

    if(arc->type == T_PIE_WEDGE_ARC) {
	bx = max2(bx,arc->center.x);
	by = max2(by,arc->center.y);
	sx = min2(sx,arc->center.x);
	sy = min2(sy,arc->center.y);
    }

    /* don't adjust by line thickness if = 1 */
    if (arc->thickness == 1)
	half_wd = 0;
    else
	half_wd = arc->thickness / 2.0 * ZOOM_FACTOR;
    *xmax = bx + half_wd;
    *ymax = by + half_wd;
    *xmin = sx - half_wd;
    *ymin = sy - half_wd;

    /* show the boundaries */
    if (appres.DEBUG && !preview_in_progress) {
	pw_vector(canvas_win, *xmin, *ymin, *xmax, *ymin, PAINT, 1, RUBBER_LINE, 0.0, RED);
	pw_vector(canvas_win, *xmax, *ymin, *xmax, *ymax, PAINT, 1, RUBBER_LINE, 0.0, RED);
	pw_vector(canvas_win, *xmax, *ymax, *xmin, *ymax, PAINT, 1, RUBBER_LINE, 0.0, RED);
	pw_vector(canvas_win, *xmin, *ymax, *xmin, *ymin, PAINT, 1, RUBBER_LINE, 0.0, RED);
    }

    /* now add in the arrow (if any) boundaries */
    arrow_bound(O_ARC, (F_line *)arc, xmin, ymin, xmax, ymax);
}

void compound_bound(F_compound *compound, int *xmin, int *ymin, int *xmax, int *ymax)
{
    active_compound_bound(compound, xmin, ymin, xmax, ymax, FALSE);
}

void active_compound_bound(F_compound *compound, int *xmin, int *ymin, int *xmax, int *ymax, Boolean active_only)
{
    F_arc	   *a;
    F_ellipse	   *e;
    F_compound	   *c;
    F_spline	   *s;
    F_line	   *l;
    F_text	   *t;
    int		    bx, by, sx, sy, first = 1;
    int		    llx, lly, urx, ury;

    if (compound == 0) {
	*xmin = *ymin = *xmax = *ymax = 0;
	return;
    }

    llx = lly = urx = ury = 0;

    for (a = compound->arcs; a != NULL; a = a->next) {
	if (active_only && !active_layer(a->depth))
	    continue;
	arc_bound(a, &sx, &sy, &bx, &by);
	if (first) {
	    first = 0;
	    llx = sx;
	    lly = sy;
	    urx = bx;
	    ury = by;
	} else {
	    llx = min2(llx, sx);
	    lly = min2(lly, sy);
	    urx = max2(urx, bx);
	    ury = max2(ury, by);
	}
    }

    for (c = compound->compounds; c != NULL; c = c->next) {
	active_compound_bound(c, &sx, &sy, &bx, &by, active_only);
	sx = c->nwcorner.x;
	sy = c->nwcorner.y;
	bx = c->secorner.x;
	by = c->secorner.y;
	if (first) {
	    first = 0;
	    llx = sx;
	    lly = sy;
	    urx = bx;
	    ury = by;
	} else {
	    llx = min2(llx, sx);
	    lly = min2(lly, sy);
	    urx = max2(urx, bx);
	    ury = max2(ury, by);
	}
    }

    for (e = compound->ellipses; e != NULL; e = e->next) {
	if (active_only && !active_layer(e->depth))
	    continue;
	ellipse_bound(e, &sx, &sy, &bx, &by);
	if (first) {
	    first = 0;
	    llx = sx;
	    lly = sy;
	    urx = bx;
	    ury = by;
	} else {
	    llx = min2(llx, sx);
	    lly = min2(lly, sy);
	    urx = max2(urx, bx);
	    ury = max2(ury, by);
	}
    }

    for (l = compound->lines; l != NULL; l = l->next) {
	if (active_only && !active_layer(l->depth))
	    continue;
	line_bound(l, &sx, &sy, &bx, &by);
	if (first) {
	    first = 0;
	    llx = sx;
	    lly = sy;
	    urx = bx;
	    ury = by;
	} else {
	    llx = min2(llx, sx);
	    lly = min2(lly, sy);
	    urx = max2(urx, bx);
	    ury = max2(ury, by);
	}
    }

    for (s = compound->splines; s != NULL; s = s->next) {
	if (active_only && !active_layer(s->depth))
	    continue;
	spline_bound(s, &sx, &sy, &bx, &by);
	if (first) {
	    first = 0;
	    llx = sx;
	    lly = sy;
	    urx = bx;
	    ury = by;
	} else {
	    llx = min2(llx, sx);
	    lly = min2(lly, sy);
	    urx = max2(urx, bx);
	    ury = max2(ury, by);
	}
    }

    for (t = compound->texts; t != NULL; t = t->next) {
	if (active_only && !active_layer(t->depth))
	    continue;
	text_bound(t, &sx, &sy, &bx, &by);
	if (first) {
	    first = 0;
	    llx = sx;
	    lly = sy;
	    urx = bx;
	    ury = by;
	} else {
	    llx = min2(llx, sx);
	    lly = min2(lly, sy);
	    urx = max2(urx, bx);
	    ury = max2(ury, by);
	}
    }

    /* round the corners to the current positioning grid */
    floor_coords(&llx, &lly);
    ceil_coords(&urx, &ury);
    *xmin = llx;
    *ymin = lly;
    *xmax = urx;
    *ymax = ury;
    /* show the boundaries */
    if (appres.DEBUG && !preview_in_progress) {
	pw_vector(canvas_win, *xmin, *ymin, *xmax, *ymin, PAINT, 1, RUBBER_LINE, 0.0, RED);
	pw_vector(canvas_win, *xmax, *ymin, *xmax, *ymax, PAINT, 1, RUBBER_LINE, 0.0, RED);
	pw_vector(canvas_win, *xmax, *ymax, *xmin, *ymax, PAINT, 1, RUBBER_LINE, 0.0, RED);
	pw_vector(canvas_win, *xmin, *ymax, *xmin, *ymin, PAINT, 1, RUBBER_LINE, 0.0, RED);
    }
}

/* basically, use the code for drawing the ellipse to find its bounds */
/* From James Tough (see u_draw.c: angle_ellipse() */

void ellipse_bound(F_ellipse *e, int *xmin, int *ymin, int *xmax, int *ymax)
{
	int	    half_wd;
	double	    c1, c2, c3, c4, c5, c6, v1, cphi, sphi, cphisqr, sphisqr;
	double	    xleft, xright, d, asqr, bsqr;
	int	    yymax, yy=0;
	float	    xcen, ycen, a, b;

	xcen = e->center.x;
	ycen = e->center.y;
	a = e->radiuses.x;
	b = e->radiuses.y;
	if (a==0 || b==0) {
	    *xmin = *xmax = xcen;
	    *ymin = *ymax = ycen;
	    return;
	}

	/* don't adjust by line thickness if = 1 */
	if (e->thickness == 1)
	    half_wd = 0;
	else
	    half_wd = e->thickness / 2.0 * ZOOM_FACTOR;

	/* angle of 0 is easy */
	if (e->angle == 0) {
	    *xmin = xcen - a - half_wd;
	    *xmax = xcen + a + half_wd;
	    *ymin = ycen - b - half_wd;
	    *ymax = ycen + b + half_wd;
	    /* show the boundaries */
	    if (appres.DEBUG && !preview_in_progress) {
		pw_vector(canvas_win,*xmin,*ymin,*xmax,*ymin, PAINT, 1, RUBBER_LINE, 0.0, RED);
		pw_vector(canvas_win,*xmax,*ymin,*xmax,*ymax, PAINT, 1, RUBBER_LINE, 0.0, RED);
		pw_vector(canvas_win,*xmax,*ymax,*xmin,*ymax, PAINT, 1, RUBBER_LINE, 0.0, RED);
		pw_vector(canvas_win,*xmin,*ymax,*xmin,*ymin, PAINT, 1, RUBBER_LINE, 0.0, RED);
	    }
	    return;
	}

	/* divide by ZOOM_FACTOR because we don't need such precision */
	a /= ZOOM_FACTOR;
	b /= ZOOM_FACTOR;
	xcen /= ZOOM_FACTOR;
	ycen /= ZOOM_FACTOR;

	cphi = cos((double)e->angle);
	sphi = sin((double)e->angle);
	cphisqr = cphi*cphi;
	sphisqr = sphi*sphi;
	asqr = a*a;
	bsqr = b*b;

	c1 = (cphisqr/asqr)+(sphisqr/bsqr);
	c2 = ((cphi*sphi/asqr)-(cphi*sphi/bsqr))/c1;
	c3 = (bsqr*cphisqr) + (asqr*sphisqr);
	yymax = sqrt(c3);
	c4 = a*b/c3;
	c5 = 0;
	v1 = c4*c4;
	c6 = 2*v1;
	c3 = c3*v1-v1;
	/* odd first points */
	*xmin = *ymin =  10000000;
	*xmax = *ymax = -10000000;
	if (yymax % 2) {
		d = sqrt(c3);
		*xmin = min2(*xmin,xcen-d);
		*xmax = max2(*xmax,xcen+d);
		*ymin = min2(*ymin,ycen);
		*ymax = max2(*ymax,ycen);
		c5 = c2;
		yy=1;
	}
	while (c3>=0) {
		d = sqrt(c3);
		xleft = c5-d;
		xright = c5+d;
		*xmin = min2(*xmin,xcen+xleft);
		*xmax = max2(*xmax,xcen+xleft);
		*ymax = max2(*ymax,ycen+yy);
		*xmin = min2(*xmin,xcen+xright);
		*xmax = max2(*xmax,xcen+xright);
		*ymax = max2(*ymax,ycen+yy);
		*xmin = min2(*xmin,xcen-xright);
		*xmax = max2(*xmax,xcen-xright);
		*ymin = min2(*ymin,ycen-yy);
		*xmin = min2(*xmin,xcen-xleft);
		*xmax = max2(*xmax,xcen-xleft);
		*ymin = min2(*ymin,ycen-yy);
		c5+=c2;
		v1+=c6;
		c3-=v1;
		yy=yy+1;
	}
	/* for simplicity, just add half the line thickness to xmax and ymax
	   and subtract half from xmin and ymin */
	*xmin = *xmin * ZOOM_FACTOR + half_wd;
	*ymin = *ymin * ZOOM_FACTOR + half_wd;
	*xmax = *xmax * ZOOM_FACTOR + half_wd;
	*ymax = *ymax * ZOOM_FACTOR + half_wd;
	/* show the boundaries */
	if (appres.DEBUG && !preview_in_progress) {
	    pw_vector(canvas_win, *xmin, *ymin, *xmax, *ymin, PAINT, 1, RUBBER_LINE, 0.0, RED);
	    pw_vector(canvas_win, *xmax, *ymin, *xmax, *ymax, PAINT, 1, RUBBER_LINE, 0.0, RED);
	    pw_vector(canvas_win, *xmax, *ymax, *xmin, *ymax, PAINT, 1, RUBBER_LINE, 0.0, RED);
	    pw_vector(canvas_win, *xmin, *ymax, *xmin, *ymin, PAINT, 1, RUBBER_LINE, 0.0, RED);
	}
}

void line_bound(F_line *l, int *xmin, int *ymin, int *xmax, int *ymax)
{
    points_bound(l->points, (l->thickness / 2), xmin, ymin, xmax, ymax);
    /* now add in the arrow (if any) boundaries */
    /* but only if there are two or more points in the line */
    if (l->points->next) {
	arrow_bound(O_POLYLINE, l, xmin, ymin, xmax, ymax);
    }
}

void spline_bound(F_spline *s, int *xmin, int *ymin, int *xmax, int *ymax)
{
    if (approx_spline(s))
	approx_spline_bound(s, xmin, ymin, xmax, ymax);
    else
	general_spline_bound(s, xmin, ymin, xmax, ymax);

    *xmax += s->thickness>>1;
    *xmin -= s->thickness>>1;
    *ymax += s->thickness>>1;
    *ymin -= s->thickness>>1;

    /* now add in the arrow (if any) boundaries */
    arrow_bound(O_SPLINE, (F_line *)s, xmin, ymin, xmax, ymax);
}

static void
general_spline_bound(F_spline *s, int *xmin, int *ymin, int *xmax, int *ymax)
{
  F_point   *cur_point, *next_point;
  F_sfactor *cur_sfactor;
  int       x0, y0, x1, y1, x2, y2 ,x ,y;

  cur_point = s->points;
  cur_sfactor = s->sfactors;
  *xmin = *xmax = x0 = x1 = cur_point->x;
  *ymin = *ymax = y0 = y1 = cur_point->y;
  next_point = cur_point->next;
  x2 = next_point->x;
  y2 = next_point->y;

  while (1)
    {
      cur_point = next_point;
      next_point = next_point->next;

      if (next_point == NULL)
	next_point = s->points;           /* usefull for closed splines,
					     no consequences on open splines */
      cur_sfactor = cur_sfactor->next;

      x0 = x1;
      y0 = y1;
      x1 = x2;
      y1 = y2;
      x2 = next_point->x;
      y2 = next_point->y;

      if (cur_sfactor->s < 0)
	{
	  x = abs(x2 - x0)>>2;
	  y = abs(y2 - y0)>>2;
	}
      else
	{
	  x = y = 0;
	}

      MIN_MAX((x1+x), (x1-x), (y1+y), (y1-y));
      if (cur_point->next==NULL)
	break;
    }
}

static void
approx_spline_bound(F_spline *s, int *xmin, int *ymin, int *xmax, int *ymax)
{
    F_point	   *p;
    int		    px, py;

    p = s->points;
    *xmin = *xmax = p->x;
    *ymin = *ymax = p->y;

    for (p=p->next ; p!=NULL ; p=p->next)
      {
	px = p->x;
	py = p->y;
	MIN_MAX(px, px, py, py);
      }
}

/* Compute the bounding box for text */
void
text_bound(F_text *t, int *xmin, int *ymin, int *xmax, int *ymax)
{
	*xmin = t->bb[0].x;
	*ymin = t->bb[0].y;
	*xmax = t->bb[1].x;
	*ymax = t->bb[1].y;
}

static void
points_bound(F_point *points, int half_wd, int *xmin, int *ymin, int *xmax, int *ymax)
{
    int		    bx, by, sx, sy;
    F_point	   *p;

    bx = sx = points->x;
    by = sy = points->y;
    for (p = points->next; p != NULL; p = p->next) {
	sx = min2(sx, p->x);
	sy = min2(sy, p->y);
	bx = max2(bx, p->x);
	by = max2(by, p->y);
    }
    half_wd *= ZOOM_FACTOR;
    *xmin = sx - half_wd;
    *ymin = sy - half_wd;
    *xmax = bx + half_wd;
    *ymax = by + half_wd;
}

int
overlapping(int xmin1, int ymin1, int xmax1, int ymax1, int xmin2, int ymin2, int xmax2, int ymax2)
{
    if (xmin1 < xmin2)
	if (ymin1 < ymin2)
	    return (xmax1 >= xmin2 && ymax1 >= ymin2);
	else
	    return (xmax1 >= xmin2 && ymin1 <= ymax2);
    else if (ymin1 < ymin2)
	return (xmin1 <= xmax2 && ymax1 >= ymin2);
    else
	return (xmin1 <= xmax2 && ymin1 <= ymax2);
}

/* extend xmin, ymin xmax, ymax by the arrow boundaries of obj (if any) */

void arrow_bound(int objtype, F_line *obj, int *xmin, int *ymin, int *xmax, int *ymax)
{
    int		    fxmin, fymin, fxmax, fymax;
    int		    bxmin, bymin, bxmax, bymax;
    F_point	   *p, *q;
    F_arc	   *a;
    int		    p1x, p1y, p2x, p2y;
    int		    dum;
    zXPoint	    arrowpts[50], arrowfillpts[50], arrowclippts[50];
    int		    npts, nfillpts, i;

    if (obj->for_arrow) {
	if (objtype == O_ARC) {
	    a = (F_arc *) obj;
	    compute_arcarrow_angle(a->center.x, a->center.y, a->point[2].x,
		       a->point[2].y, a->direction, a->for_arrow, &p1x, &p1y);
	    p2x = a->point[2].x;	/* forward tip */
	    p2y = a->point[2].y;
	} else {
	    /* this doesn't work very well for a spline with few points
		and lots of curvature */
	    /* locate last point (forward tip) and next-to-last point */
	    for (p = obj->points; p->next; p = p->next)
		q = p;
	    p1x = q->x;
	    p1y = q->y;
	    p2x = p->x;
	    p2y = p->y;
	}
	calc_arrow(p1x, p1y, p2x, p2y, obj->thickness,
			obj->for_arrow, arrowpts, &npts, arrowfillpts, &nfillpts, arrowclippts, &dum);
	fxmin=fymin=10000000;
	fxmax=fymax=-10000000;
	for (i=0; i<npts; i++) {
	    fxmin = min2(fxmin, arrowpts[i].x);
	    fymin = min2(fymin, arrowpts[i].y);
	    fxmax = max2(fxmax, arrowpts[i].x);
	    fymax = max2(fymax, arrowpts[i].y);
	}
	*xmin = min2(*xmin, fxmin);
	*xmax = max2(*xmax, fxmax);
	*ymin = min2(*ymin, fymin);
	*ymax = max2(*ymax, fymax);
	if (appres.DEBUG && !preview_in_progress) {
	  pw_vector(canvas_win,fxmin,fymin,fxmax,fymin,PAINT,1,RUBBER_LINE,0.0,MAGENTA);
	  pw_vector(canvas_win,fxmax,fymin,fxmax,fymax,PAINT,1,RUBBER_LINE,0.0,MAGENTA);
	  pw_vector(canvas_win,fxmax,fymax,fxmin,fymax,PAINT,1,RUBBER_LINE,0.0,MAGENTA);
	  pw_vector(canvas_win,fxmin,fymax,fxmin,fymin,PAINT,1,RUBBER_LINE,0.0,MAGENTA);
	}
    }
    if (obj->back_arrow) {
	if (objtype == O_ARC) {
	    a = (F_arc *) obj;
	    compute_arcarrow_angle(a->center.x, a->center.y, a->point[0].x,
		       a->point[0].y, a->direction ^ 1, a->back_arrow, &p1x, &p1y);
	    p2x = a->point[0].x;	/* backward tip */
	    p2y = a->point[0].y;
	} else {
	    p1x = obj->points->next->x;	/* second point */
	    p1y = obj->points->next->y;
	    p2x = obj->points->x;	/* first point (forward tip) */
	    p2y = obj->points->y;
	}
	calc_arrow(p1x, p1y, p2x, p2y, obj->thickness,
			obj->back_arrow, arrowpts, &npts, arrowfillpts, &nfillpts, arrowclippts, &dum);
	bxmin=bymin=10000000;
	bxmax=bymax=-10000000;
	for (i=0; i<npts; i++) {
	    bxmin = min2(bxmin, arrowpts[i].x);
	    bymin = min2(bymin, arrowpts[i].y);
	    bxmax = max2(bxmax, arrowpts[i].x);
	    bymax = max2(bymax, arrowpts[i].y);
	}
	*xmin = min2(*xmin, bxmin);
	*xmax = max2(*xmax, bxmax);
	*ymin = min2(*ymin, bymin);
	*ymax = max2(*ymax, bymax);
	if (appres.DEBUG && !preview_in_progress) {
	  pw_vector(canvas_win,bxmin,bymin,bxmax,bymin,PAINT,1,RUBBER_LINE,0.0,MAGENTA);
	  pw_vector(canvas_win,bxmax,bymin,bxmax,bymax,PAINT,1,RUBBER_LINE,0.0,MAGENTA);
	  pw_vector(canvas_win,bxmax,bymax,bxmin,bymax,PAINT,1,RUBBER_LINE,0.0,MAGENTA);
	  pw_vector(canvas_win,bxmin,bymax,bxmin,bymin,PAINT,1,RUBBER_LINE,0.0,MAGENTA);
	}
    }
}
