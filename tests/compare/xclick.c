/* xclick x y [button] | xclick key <keysym> [mods] - synthesize input with XTest */
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(int argc, char **argv) {
	Display *d = XOpenDisplay(NULL);
	if (!d || argc < 3) { fprintf(stderr, "usage: xclick x y [button] | key keysym [shift|ctrl|meta]\n"); return 1; }
	if (!strcmp(argv[1], "drag") && argc >= 6) {
		/* press at x1 y1, move to x2 y2, release: spring-loaded menus */
		XTestFakeMotionEvent(d, -1, atoi(argv[2]), atoi(argv[3]), 0);
		XSync(d, False); usleep(50000);
		XTestFakeButtonEvent(d, 1, True, 0); XSync(d, False); usleep(300000);
		XTestFakeMotionEvent(d, -1, atoi(argv[4]), atoi(argv[5]), 0);
		XSync(d, False); usleep(300000);
		XTestFakeButtonEvent(d, 1, False, 0);
	} else if (!strcmp(argv[1], "move") && argc >= 4) {
		XTestFakeMotionEvent(d, -1, atoi(argv[2]), atoi(argv[3]), 0);
	} else if (!strcmp(argv[1], "key")) {
		KeyCode mod = 0;
		if (argc > 3) mod = XKeysymToKeycode(d, !strcmp(argv[3], "shift") ? XK_Shift_L : !strcmp(argv[3], "ctrl") ? XK_Control_L : XK_Meta_L);
		KeyCode k = XKeysymToKeycode(d, XStringToKeysym(argv[2]));
		if (mod) XTestFakeKeyEvent(d, mod, True, 0);
		XTestFakeKeyEvent(d, k, True, 0); XTestFakeKeyEvent(d, k, False, 0);
		if (mod) XTestFakeKeyEvent(d, mod, False, 0);
	} else {
		int b = argc > 3 ? atoi(argv[3]) : 1;
		XTestFakeMotionEvent(d, -1, atoi(argv[1]), atoi(argv[2]), 0);
		XSync(d, False); usleep(50000);
		XTestFakeButtonEvent(d, b, True, 0); XTestFakeButtonEvent(d, b, False, 0);
	}
	XSync(d, False); XCloseDisplay(d); return 0;
}
