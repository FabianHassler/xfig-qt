/*
 * fallback.c - X resources that xfig's app-defaults file (Fig) sets and
 * xfig-qt needs: xfig-qt has no app-defaults file, and its own defaults
 * for colors and fonts.  "Fig.name" is an application resource, other
 * patterns name widgets (shim/xt_widget.c).
 */
#include "xshim.h"

String fallback_resources[] = {
	"Fig*file_panel*mask*string:	*.fig *.fig.*",
	"Fig*browse_panel*mask*string:	*.gif* *.jpg* *.jpeg* *.pcx* *.png* *.ppm* *.tif* *.xpm* *.xbm* *.ps* *.eps* *.pdf*",
	/* the update markers in the indicator buttons (Fig-color) */
	"Fig*button_form.update*background:	black",
	"Fig*button_form.update*foreground:	red",
#ifdef __APPLE__
	/* the help, picture editing: macOS's open, not xdg-open */
	"Fig.image_editor:	open",
	"Fig.browser:	open %f",
	"Fig.pdfviewer:	open %f",
#endif
	NULL
};
