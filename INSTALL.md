# Installing xfig-qt

xfig-qt builds with CMake on macOS and Linux. It needs:

| What | Why |
|---|---|
| Qt 6 (Widgets, PrintSupport, Svg), 6.4 or newer | the user interface; Svg for PDF and bitmaps without Ghostscript |
| QtPdf (optional) | previews of PDF pictures without Ghostscript |
| fontconfig, FreeType | xfig's text, with the same font matching as X11 xfig |
| libpng, libjpeg, libtiff (optional) | picture objects |
| CUPS | printing |
| a C11 and C++17 compiler, CMake 3.21, pkg-config | building |

and, to run fully:

| What | Why |
|---|---|
| **fig2dev** (3.2.9 or newer) | export and printing, as in X11 xfig |
| **Ghostscript** (`gs`), recommended | PDF and bitmap export and printing made from fig2dev's PostScript; previews of EPS pictures |
| the URW base-35 fonts | xfig's PostScript fonts (Times, Helvetica, ...); macOS has suitable fonts |

Without fig2dev xfig-qt still draws, edits, loads and saves figures.

Without Ghostscript, PDF, PNG, JPEG, TIFF, PPM, XBM and XPM export and
printing work too: Qt draws fig2dev's SVG output instead of Ghostscript
turning its PostScript into PDF or pixels. fig2dev's SVG and PostScript do
not quite agree, so Ghostscript remains the better choice; the message
panel says when the SVG route was taken. Without Ghostscript there is no
GIF, PCX or combined PDF/LaTeX export, and EPS pictures show as a frame.
PDF pictures still get their preview, from QtPdf.

After installing, `xfig figure.fig` starts it; xfig's command line options
work (`-zoom 2`, `-metric`, `-startfontsize 14`, `-xrm 'Fig.resource:
value'`...).

## macOS

### With Homebrew

```sh
brew install fabianhassler/tap/xfig-qt
```

This builds xfig-qt and installs `xfig` and the app `xfig.app`; fig2dev and
Ghostscript come along (`--without-ghostscript` leaves Ghostscript out).
If you installed one of the first releases (3.2.9a-qt1 to -qt3), use
`brew reinstall xfig-qt` once: Homebrew counts the new 1.0 versions as
older than those. To start it from the Finder or Launchpad:

```sh
ln -sf "$(brew --prefix xfig-qt)/xfig.app" /Applications/Xfig.app
```

Homebrew's X11 `xfig` formula also installs a `xfig` command: if you have
it, `brew unlink xfig` first (or uninstall it).

### From source

```sh
brew install cmake pkgconf qt fontconfig freetype libpng jpeg-turbo libtiff \
	fig2dev ghostscript		# ghostscript: recommended
git clone https://github.com/FabianHassler/xfig-qt.git
cd xfig-qt
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j8
open build/xfig.app --args ~/some/figure.fig
```

`build/xfig.app` runs from the build directory (it finds xfig's data in
the source tree). To install it with the data inside the app:

```sh
cmake --install build --prefix ~/Applications	# ~/Applications/xfig.app
```

## Linux

Install the packages, then build and install with CMake (below). CI
builds on Ubuntu 24.04; the Fedora and Arch package lists are not tested
there.

Debian, Ubuntu (24.04 or newer):

```sh
sudo apt install build-essential cmake pkg-config qt6-base-dev qt6-qpa-plugins \
	qt6-svg-dev qt6-pdf-dev libfontconfig-dev libfreetype-dev libpng-dev \
	libjpeg-dev libtiff-dev libcups2-dev fig2dev ghostscript fonts-urw-base35
```

Fedora:

```sh
sudo dnf install gcc gcc-c++ cmake pkgconf qt6-qtbase-devel qt6-qtsvg-devel \
	qt6-qtpdf-devel fontconfig-devel \
	freetype-devel libpng-devel libjpeg-turbo-devel libtiff-devel cups-devel \
	fig2dev ghostscript urw-base35-fonts
```

Arch Linux:

```sh
sudo pacman -S base-devel cmake pkgconf qt6-base qt6-svg qt6-webengine fontconfig freetype2 libpng \
	libjpeg-turbo libtiff libcups fig2dev ghostscript gsfonts
```

Build and install (to `/usr/local`):

```sh
git clone https://github.com/FabianHassler/xfig-qt.git
cd xfig-qt
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
sudo cmake --install build
```

This installs `xfig` in `/usr/local/bin`, xfig's data in
`/usr/local/share/xfig-qt`, the man page `xfig(1)`, and a desktop entry and
icon, so xfig appears in the applications menu. Use
`-DCMAKE_INSTALL_PREFIX=$HOME/.local` for an installation in your home
directory, without `sudo`. `build/xfig` also runs without installing.

If your distribution packages xfig (the X11 version), both install a
`xfig` command; install xfig-qt to another prefix or uninstall xfig.

## Windows

There is no native Windows version: fig2dev, which xfig needs for export
and printing, does not build on Windows. Windows 10 and 11 can run the
Linux version in WSL2, whose WSLg shows Linux programs on the Windows
desktop: install Ubuntu (`wsl --install`), then follow the Debian/Ubuntu
instructions above inside it. This is not tested.

## Checking an installation

`tests/smoke_test.sh path/to/xfig` (from the sources) runs xfig headless:
it finds xfig's data, opens, saves and prints a figure to PDF, and reports
each step.
