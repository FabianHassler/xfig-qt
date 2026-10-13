# xfig-qt — xfig 3.2.9a on Qt 6, without X11

This runs the original xfig 3.2.9a source code (in `core/`) as a native Qt 6
application on macOS. No XQuartz, Xlib, Xt or Xaw is needed.

## Build and run

```sh
brew install qt fontconfig freetype libpng jpeg libtiff fig2dev
cmake -S . -B build -DCMAKE_PREFIX_PATH=$(brew --prefix qt)
cmake --build build -j8
open build/xfig.app --args ~/some/figure.fig     # or run the binary directly:
build/xfig.app/Contents/MacOS/xfig figure.fig
```

xfig's command-line options still work (`-zoom 2`, `-metric`,
`-startfontsize 14`, `-splash`, `-xrm 'Fig.resource: value'`, …).
fig2dev must be on the `PATH` for export and printing, as with X11 xfig.

## How it works

xfig's code is unchanged. The X11 libraries it calls are replaced underneath it.

| Layer | Files | What it does |
|---|---|---|
| X11 headers | `shim/xshim.h`, `shim/xtshim.h`, `shim/X11/*` | The subset of Xlib/Xt/Xaw/Xft types and functions xfig uses |
| Xlib on Qt | `platform/xlib_qt.cpp` | Windows and pixmaps are `QImage`s. GCs, XOR rubber-banding, stipples, regions and images are drawn with `QPainter`. Xft text uses fontconfig (same font matching as before) plus FreeType outlines. Retina resolution |
| Xt event loop | `platform/xt_qt.cpp` | Timers, input, pointer and keyboard queries and cursors run on the Qt event loop |
| Xt widgets | `shim/xt_widget.c`, `shim/xt_translate.c` | Widget records with resources, callbacks and event handlers. Xt action tables and **translation tables** are parsed and dispatched |
| Xaw on Qt | `ui/xaw.cpp` | Every Athena widget gets a Qt peer: Form/Box layouts, Command/Toggle/Label, `QLineEdit` text fields, `QListWidget` lists, scrollbars, viewports. Shells become windows, SimpleMenus become `QMenu`s |
| Main window | `ui/mainwindow.cpp` | Arranges xfig's panels (mode panel, rulers, canvas, indicator panel, depth manager). Builds the **native menu bar** from xfig's own menus, live, with ⌘ shortcuts |
| Glue | `ui/bridge.c`, `ui/main.cpp` | Starts Qt, then runs xfig's own `main()` |

Because xfig's own event code runs, all drawing and editing modes, keyboard
input, undo, the object edit dialog and the other panels behave as in the
X11 version.

### macOS input mapping

| Mac | xfig (X11) |
|---|---|
| click | left button |
| right click / two-finger click | right button |
| ⌘-click or middle button | middle button |
| Control | Ctrl |
| Option | Meta (Alt) |
| two-finger scroll | pan (Shift: horizontal) |

## Testing

Run headless with `QT_QPA_PLATFORM=offscreen`. `XFIGQT_SCRIPT=file` drives
the program with synthesized input; `ui/testscript.cpp` lists the commands.
`XFIGQT_SNAPSHOT=file.png` saves the window after startup.

```
mode 7                 # box mode (mode numbers from core/mode.h)
click 100 100
move 200 160
click 300 220
menu File Export
snapwin dialog.png export_popup
save out.fig
quit
```

For an AddressSanitizer build, configure with
`-DCMAKE_C_FLAGS=-fsanitize=address -DCMAKE_CXX_FLAGS=-fsanitize=address`
and run with `ASAN_OPTIONS=allow_user_segv_handler=0`.

## Status

Verified with test scripts:
- figures load and render: lines, splines, arcs, text in PostScript fonts,
  dashes, page border, rulers;
- box, circle, polyline and text drawn interactively and saved to a correct
  `.fig` file;
- native menus; mode panel; indicator panel, which switches with the mode;
- object edit dialog; export dialog.

Known gaps (next steps):
- the UI is forced to light mode: xfig's icons are black bitmaps. A dark
  theme would need recoloured icons;
- xfig's layouts use fixed pixel sizes made for X11 fonts. Labels and
  buttons grow to fit their text, but some dialogs may still look cramped;
- XPM images are not read (`f_readxpm.c` needs libXpm; add a Qt-based reader);
- no input method support for composed non-ASCII text input yet;
- not yet done: native file dialogs instead of xfig's Xaw file browser, a
  modern toolbar/dock look for the mode and indicator panels,
  trackpad pinch zoom, and a bundled fig2dev and fonts in the `.app`.
