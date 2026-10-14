# Installing xfig-qt

xfig-qt builds with CMake on macOS, Linux and Windows. It needs:

| What | Why |
|---|---|
| Qt 6 (Widgets, PrintSupport), 6.4 or newer | the user interface |
| fontconfig, FreeType | xfig's text, with the same font matching as X11 xfig |
| libpng, libjpeg, libtiff (optional) | picture objects |
| CUPS (macOS, Linux) | printing |
| a C11 and C++17 compiler, CMake 3.21, pkg-config | building |

and, to run fully:

| What | Why |
|---|---|
| **fig2dev** (3.2.9 or newer) | export and printing, as in X11 xfig |
| **Ghostscript** (`gs`) | printing, and the previews of EPS and PDF pictures |
| the URW base-35 fonts | xfig's PostScript fonts (Times, Helvetica, ...); macOS and Windows have suitable fonts |

Without fig2dev and Ghostscript xfig-qt still draws, edits, loads and
saves figures.

After installing, `xfig figure.fig` starts it; xfig's command line options
work (`-zoom 2`, `-metric`, `-startfontsize 14`, `-xrm 'Fig.resource:
value'`...).

## macOS

### With Homebrew

```sh
brew install fabianhassler/tap/xfig-qt
```

This builds xfig-qt and installs `xfig` and the app `xfig.app`; fig2dev and
Ghostscript come along. To start it from the Finder or Launchpad:

```sh
ln -sf "$(brew --prefix xfig-qt)/xfig.app" /Applications/Xfig.app
```

Homebrew's X11 `xfig` formula also installs a `xfig` command: if you have
it, `brew unlink xfig` first (or uninstall it).

### From source

```sh
brew install cmake pkgconf qt fontconfig freetype libpng jpeg-turbo libtiff \
	fig2dev ghostscript
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
	libfontconfig-dev libfreetype-dev libpng-dev libjpeg-dev libtiff-dev \
	libcups2-dev fig2dev ghostscript fonts-urw-base35
```

Fedora:

```sh
sudo dnf install gcc gcc-c++ cmake pkgconf qt6-qtbase-devel fontconfig-devel \
	freetype-devel libpng-devel libjpeg-turbo-devel libtiff-devel cups-devel \
	fig2dev ghostscript urw-base35-fonts
```

Arch Linux:

```sh
sudo pacman -S base-devel cmake pkgconf qt6-base fontconfig freetype2 libpng \
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

xfig-qt builds with MSYS2's MinGW-w64 tools (UCRT64).

1. Install MSYS2 from <https://www.msys2.org> and open the
   **MSYS2 UCRT64** shell.
2. Install the tools and libraries:

   ```sh
   pacman -Syu
   pacman -S git mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
   	mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-pkgconf \
   	mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-fontconfig \
   	mingw-w64-ucrt-x86_64-freetype mingw-w64-ucrt-x86_64-libpng \
   	mingw-w64-ucrt-x86_64-libjpeg-turbo mingw-w64-ucrt-x86_64-libtiff \
   	mingw-w64-ucrt-x86_64-libiconv mingw-w64-ucrt-x86_64-ghostscript
   ```

3. Build and install, here to `C:\xfig-qt`:

   ```sh
   git clone https://github.com/FabianHassler/xfig-qt.git
   cd xfig-qt
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   cmake --install build --prefix /c/xfig-qt
   ```

4. Start `xfig` from the UCRT64 shell, which has the libraries on its PATH:
   `/c/xfig-qt/bin/xfig`. To start `C:\xfig-qt\bin\xfig.exe` from Explorer
   too, copy the Qt and other libraries next to it:

   ```sh
   windeployqt6 /c/xfig-qt/bin/xfig.exe
   ldd /c/xfig-qt/bin/xfig.exe | grep -i ucrt64 | awk '{print $3}' | \
   	xargs -I{} cp {} /c/xfig-qt/bin/
   ```

On Windows:

- **fig2dev** has no MSYS2 package. Without it, export and printing do not
  work; drawing, editing, loading and saving do.
- Printing goes through the Windows print dialog. Ghostscript renders the
  pages at the printer's resolution, so the print is an image, not vector.
- xfig's pre-edit window for input methods and editing text in an external
  editor are not available (they need `fork()`).
- `~user/` in file names is not expanded; `~/` is.

## Checking an installation

`tests/smoke_test.sh path/to/xfig` (from the sources) runs xfig headless:
it finds xfig's data, opens, saves and prints a figure to PDF, and reports
each step.
