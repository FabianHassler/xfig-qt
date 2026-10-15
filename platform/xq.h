/*
 * xq.h - glue between the Xlib emulation (xlib_qt.cpp) and the Qt UI.
 *
 * Every X drawable (window or pixmap) is a QImage.  The UI owns the widget
 * that shows a "window" drawable and is told when its contents changed.
 */
#ifndef XQ_H
#define XQ_H

#ifdef __cplusplus
/*
 * X11 defines macros such as None, Bool, KeyPress and Expose that clash with
 * Qt identifiers: include all of Qt that the port uses before the shim.
 */
#include <QtWidgets/QtWidgets>
#include <QtCore/qmetatype.h>
#include <functional>
/* Qt names that X11 macros hide, captured before the shim */
namespace qtev {
constexpr QEvent::Type KeyPressEvent = QEvent::KeyPress;
constexpr QEvent::Type KeyReleaseEvent = QEvent::KeyRelease;
}
#endif
#include "xshim.h"

#ifdef __cplusplus

/* the backing image of a drawable, or nullptr */
QImage	*xq_image(Drawable d);

/* 1 for bitmaps, 24 for pixmaps and windows, 0 if there is no such drawable */
int	xq_drawable_depth(Drawable d);

/* called whenever a window drawable was painted on */
void	xq_set_damage_callback(std::function<void(Window)> cb);

/* the cursor requested for a window by XDefineCursor */
void	xq_set_cursor_callback(std::function<void(Window, Cursor)> cb);

/* the Qt cursor for an X cursor */
QCursor	xq_qcursor(Cursor c);

/* the Qt font of an X core font */
QFont	xq_qfont(XFontStruct *fs);

extern "C" {
#endif

extern Display	*xq_display;

/* end the process at once (xfig's exit(), see CMakeLists.txt) */
void	xq_exit(int status) __attribute__((noreturn));

/* create a window drawable with the given logical size and device ratio */
Window	xq_create_window(int width, int height, double dpr);
void	xq_resize_window(Window w, int width, int height, double dpr);

#ifdef __cplusplus
}
#endif

#endif /* XQ_H */
