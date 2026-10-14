#!/usr/bin/env python3
"""
windows.py - open xfig's dialogs in the original X11 xfig and in xfig-qt
and show them side by side, to compare them by eye.

The X11 xfig runs on a private Xvfb display and is driven with XTest
(xclick.c, compiled on first use): a dialog is opened by dragging through
its menu.  xfig-qt runs offscreen and opens the same dialog with xfig's own
menu callback (the xmenu script command).

	tests/compare/windows.py [-o build/windows] [name...]

The report is build/windows/index.html.
"""

import argparse
import html
import os
import re
import subprocess
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
X11BIN = Path("/opt/X11/bin")
OLD_XFIG = "/opt/homebrew/bin/xfig"
QT_XFIG = ROOT / "build/xfig.app/Contents/MacOS/xfig"
FIG = ROOT / "data/Libraries/Examples/pictures.fig"

# the X11 xfig's window is placed right, away from its popups at 0,0
OFFX = 680
# the menu buttons' x, and the y of each entry in the dropped menu
MENU_X = {"File": 30, "Edit": 92, "View": 152, "Snap": 212, "Help": 272}

# name, menu, entry y in the X11 xfig's menu, entry for xfig-qt's xmenu
DIALOGS = [
	("open", "File", 52, "Open"),
	("merge", "File", 67, "Merge"),
	("save_as", "File", 97, "Save As"),
	("export", "File", 112, "Export"),
	("print", "File", 127, "Print"),
	("search_replace", "Edit", 83, "Search/Replace"),
	("global_settings", "Edit", 131, "Global settings"),
	("units", "Edit", 148, "Set units"),
	("styles", "View", 35, "Manage Styles"),
	("character_map", "View", 163, "Character map"),
	("about", "Help", 99, "About"),
]


def run(cmd, env=None, timeout=60):
	return subprocess.run(cmd, env=env, timeout=timeout, capture_output=True)


def xclick():
	exe = HERE / "build" / "xclick"
	src = HERE / "xclick.c"
	if not exe.exists() or exe.stat().st_mtime < src.stat().st_mtime:
		exe.parent.mkdir(exist_ok=True)
		run(["cc", src, "-o", exe, "-I/opt/X11/include", "-L/opt/X11/lib",
		     "-lX11", "-lXtst"])
	return exe


def windows(env):
	"""The mapped top-level windows: id -> (instance name, title)."""
	out = run([X11BIN / "xwininfo", "-root", "-children"], env=env).stdout.decode()
	wins = {}
	for m in re.finditer(r'(0x[0-9a-f]+) "([^"]*)": \("([^"]*)" "Fig"\)', out):
		info = run([X11BIN / "xwininfo", "-id", m.group(1)], env=env).stdout.decode()
		if "IsViewable" in info:
			wins[m.group(1)] = (m.group(3), m.group(2))
	return wins


def capture(env, win, png):
	with tempfile.TemporaryDirectory() as tmp:
		xwd = Path(tmp) / "w.xwd"
		run([X11BIN / "xwd", "-id", win, "-silent", "-out", xwd], env=env)
		run(["ffmpeg", "-loglevel", "error", "-y", "-i", xwd, "-pix_fmt",
		     "rgb24", png])


def old_dialog(env, menu, y, png):
	"""Open the dialog in a fresh X11 xfig; return its window's names."""
	proc = subprocess.Popen([OLD_XFIG, "-geometry", f"1200x850+{OFFX}+0",
			str(FIG)], env=env, cwd=FIG.parent,
			stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
	try:
		for _ in range(60):
			time.sleep(0.25)
			if any(t.startswith("Xfig - ") for _, t in windows(env).values()):
				break
		time.sleep(1)
		before = set(windows(env))
		x = OFFX + MENU_X[menu]
		run([xclick(), "drag", str(x), "11", str(x + 20), str(y)], env=env)
		time.sleep(1.5)
		new = {w: n for w, n in windows(env).items() if w not in before}
		new = {w: n for w, n in new.items() if n[0] != "file_msg"}
		if not new:
			return None
		win = next(iter(new))
		capture(env, win, png)
		return new[win]
	finally:
		proc.kill()
		proc.wait()


def qt_dialog(menu, entry, png):
	with tempfile.TemporaryDirectory() as tmp:
		script = Path(tmp) / "s.txt"
		script.write_text(f"open {FIG}\nwait 300\nxmenu {menu} {entry}\n"
				f"wait 500\nsnapwin {png}\nquit\n")
		env = dict(os.environ, QT_QPA_PLATFORM="offscreen",
				XFIGQT_SCRIPT=str(script))
		run([QT_XFIG], env=env)
	return Path(png).exists()


def main():
	ap = argparse.ArgumentParser(description=__doc__,
			formatter_class=argparse.RawDescriptionHelpFormatter)
	ap.add_argument("-o", "--out", type=Path, default=ROOT / "build/windows")
	ap.add_argument("names", nargs="*", help="only these dialogs")
	args = ap.parse_args()
	out = args.out.resolve()
	out.mkdir(parents=True, exist_ok=True)

	num = next(n for n in range(140, 400)
		   if not Path(f"/tmp/.X11-unix/X{n}").exists())
	xvfb = subprocess.Popen([X11BIN / "Xvfb", f":{num}", "-screen", "0",
			"1900x1000x24", "-nolisten", "tcp"],
			stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
	env = dict(os.environ, DISPLAY=f":{num}")
	time.sleep(2)
	rows = []
	try:
		for name, menu, y, entry in DIALOGS:
			if args.names and name not in args.names:
				continue
			old = old_dialog(env, menu, y, out / f"{name}_old.png")
			qt = qt_dialog(menu, entry, out / f"{name}_qt.png")
			print(f"{name:16} X11: {old[1] if old else 'none':30} "
			      f"Qt: {'ok' if qt else 'none'}", flush=True)
			rows.append((name, menu, entry, old))
	finally:
		xvfb.terminate()
		xvfb.wait()

	parts = ["<!doctype html><meta charset=utf-8><title>xfig dialogs</title>",
		 "<style>body{font:14px sans-serif;margin:16px}td{vertical-align:top;"
		 "padding:4px 12px 16px 0}img{border:1px solid #ccc;max-width:48vw}"
		 "</style><h1>xfig dialogs: X11 xfig (left), xfig-qt (right)</h1>"
		 "<table>"]
	for name, menu, entry, old in rows:
		parts.append(f"<tr><td colspan=2><b>{html.escape(menu)} &gt; "
			     f"{html.escape(entry)}</b> "
			     f"{html.escape(old[0]) if old else ''}</td></tr><tr>"
			     f"<td><img src='{name}_old.png'></td>"
			     f"<td><img src='{name}_qt.png'></td></tr>")
	parts.append("</table>")
	(out / "index.html").write_text("\n".join(parts))
	print(f"report: {out / 'index.html'}")


if __name__ == "__main__":
	main()
