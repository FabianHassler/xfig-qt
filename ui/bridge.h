/*
 * bridge.h - calls from the Qt user interface into xfig's C code.
 */
#ifndef BRIDGE_H
#define BRIDGE_H

#include "xshim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* xfig's original main(), renamed */
int	xfig_main(int argc, char **argv);

void	xqb_canvas_resized(int width, int height);
void	xqb_canvas_sync(int width, int height);
void	xqb_rulers_resized(int topruler_wd, int sideruler_ht);
void	xqb_redraw(void);
int	xqb_load_file(const char *path);
int	xqb_save_file(const char *path);
const char *xqb_current_file(void);
const char *xqb_current_dir(void);
int	xqb_figure_modified(void);
double	xqb_zoom(void);
void	xqb_set_zoom(double z);
void	xqb_zoom_to_fit(void);
void	xqb_unzoom(void);
Widget	xqb_canvas_widget(void);
Widget	xqb_msg_widget(void);
int	xqb_num_modes(void);
int	xqb_num_menus(void);
const char *xqb_menu_title(int i);
Widget	xqb_menu_widget(int i);
void	xqb_quit(void);
Widget	xqb_mode_button(int mode);
Widget	xqb_tool_widget(void);

#ifdef __cplusplus
}
/* the top widgets: the main shell and all popup shells */
QList<Widget> xqb_root_widgets();
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif
