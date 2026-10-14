# xfig-qt — xfig 3.2.9a on Qt 6, without X11

This runs the original xfig 3.2.9a source code (in `core/`) as a native Qt 6
application on macOS and Linux. No X server, Xlib, Xt or Xaw is needed.

## Installation

On macOS with Homebrew:

```sh
brew install fabianhassler/tap/xfig-qt
```

On Linux, and from source on macOS, see [INSTALL.md](INSTALL.md): it lists
the packages for Debian/Ubuntu, Fedora and Arch and the CMake build. In
short:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
build/xfig figure.fig			# Linux
open build/xfig.app --args figure.fig	# macOS
```

xfig's command-line options still work (`-zoom 2`, `-metric`,
`-startfontsize 14`, `-splash`, `-xrm 'Fig.resource: value'`, …).
Export and printing need fig2dev; Ghostscript is recommended. Without it,
PDF, bitmap export and printing are drawn by Qt from fig2dev's SVG
(see [INSTALL.md](INSTALL.md)).

## How it works

xfig's code is unchanged. The X11 libraries it calls are replaced underneath it.

| Layer | Files | What it does |
|---|---|---|
| X11 headers | `shim/xshim.h`, `shim/xtshim.h`, `shim/X11/*` | The subset of Xlib/Xt/Xaw/Xft types and functions xfig uses |
| XPM | `shim/xpm.c` | The part of libXpm xfig uses: an XPM 3 reader, used for XPM picture objects |
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

### Comparing with X11 xfig

`tests/compare/compare.py` opens `.fig` files in the original X11 xfig
(Homebrew's `xfig`, on private `Xvfb` displays from XQuartz) and in
xfig-qt, and compares the two canvases pixel by pixel, allowing a
one-pixel shift. It needs XQuartz, `ffmpeg`, and Python with numpy and
Pillow.

```sh
tests/compare/compare.py -j 8 ../old                  # every figure (~2500)
tests/compare/compare.py --sample 200 ../old          # a random sample
tests/compare/compare.py some.fig other.fig
```

The report goes to `build/compare/index.html` (old, Qt and a red
difference image per figure, worst first) and `results.csv`. The score
is the fraction of inked pixels that differ; the exit status is 1 if any
figure scores above `--fail` (default 0.05).

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
- File → Open…, Merge… and Save As… use the system's file dialog, and Save
  for a figure without a name too. Before a figure is replaced, an
  alert offers Save, Don't Save or Cancel. xfig's file panel (with its
  figure preview) is still there through xfig's keyboard accelerators;
- File → Print… opens the system's print dialog (printer, copies, pages,
  two-sided, grayscale, Save as PDF). xfig's PostScript, made with its print
  settings, becomes a PDF (Ghostscript) that goes to CUPS, so the
  print stays vector. Without Ghostscript, Qt prints fig2dev's SVG.
  File → Print Options… is xfig's print panel, for
  magnification, layers, grid, background and multiple pages;
- object edit dialog; export dialog;
- all 2561 figures in `old/` compared with X11 xfig (`tests/compare`):
  2556 render the same. The rest are small: dashes along arcs (X draws
  them shorter than xfig asks), rotated text about 2.5% wider, and a
  half-pixel offset of a thin line drawn inside a thick one.

Known gaps (next steps):
- the UI is forced to light mode: xfig's icons are black bitmaps. A dark
  theme would need recoloured icons;
- xfig's layouts use fixed pixel sizes made for X11 fonts. Labels and
  buttons grow to fit their text, but some dialogs may still look cramped;
- no input method support for composed non-ASCII text input yet;
- the Export panel's file name through the system's save dialog (File →
  Open…, Merge…, Save As… already use the system's file dialog);
- not yet done: a modern toolbar/dock look for the mode and indicator
  panels, trackpad pinch zoom, and a bundled fig2dev and fonts in the `.app`.
