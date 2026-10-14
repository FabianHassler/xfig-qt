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

# the X11 xfig's window is placed right, away from its popups at 0,0;
# all positions below are for its 1200x850 window
OFFX = 680
# the menu buttons' x, and the y of each entry in the dropped menu
MENU_X = {"File": 30, "Edit": 92, "View": 152, "Snap": 212, "Help": 272}


def menu(name, menu, y, entry):
	"""A dialog from a menu: the entry's y in the X11 xfig's menu, the
	entry for xfig-qt's xmenu."""
	x = OFFX + MENU_X[menu]
	return (name, f"{menu} > {entry}",
		[("drag", x, 11, x + 20, y)], [f"xmenu {menu} {entry}"], FIG)


# mode buttons in the X11 xfig's mode panel (window coordinates), and the
# mode numbers of mode.h for xfig-qt
MODES = {"box": ((18, 226), 7), "text": ((56, 290), 9),
	 "polyline": ((56, 190), 6)}
# the indicators' x in the X11 xfig, in the order shown, per mode
INDICATORS = {
	"box": ["zoom", "grid", "point_posn", "depth", "pen_color",
		"fill_color", "fill_style", "line_width", "line_style",
		"join_style", "cap_style"],
	"text": ["zoom", "grid", "point_posn", "depth", "pen_color",
		 "text_just", "angle", "text_flags", "text_size", "text_step",
		 "text_font"],
	"polyline": ["zoom", "grid", "point_posn", "depth", "angle_geom",
		     "pen_color", "fill_color", "fill_style", "line_width",
		     "line_style", "join_style", "cap_style", "arrow_mode",
		     "arrow_type", "arrow_size", "dimension_line"],
}
IND_X = {"box": [30, 93, 158, 220, 295, 387, 467, 532, 598, 664, 730],
	 "text": [30, 93, 158, 220, 295, 373, 453, 537, 615, 683, 867],
	 "polyline": [30, 93, 158, 220, 283, 362, 455, 532, 597, 663, 731,
		      797, 861, 927, 998, 1080]}
IND_Y = 813


def indicator(mode, k):
	(mx, my), num = MODES[mode]
	return (f"{mode}_{INDICATORS[mode][k]}",
		f"{mode} mode > {INDICATORS[mode][k].replace('_', ' ')}",
		[("click", OFFX + mx, my), ("click", OFFX + IND_X[mode][k], IND_Y)],
		[f"mode {num}", "wait 200", f"indbutton {k}"], FIG)


# the canvas in the X11 xfig's window
CANVAS = (75, 74)


def canvas(x, y):
	return (OFFX + CANVAS[0] + x, CANVAS[1] + y)


OBJECTS = HERE / "figs/objects.fig"


def edit(name, x, y, fig=OBJECTS):
	"""The edit dialog of the object at canvas x, y of fig."""
	return (f"edit_{name}", f"Edit mode > {name}",
		[("click", OFFX + 18, 620), ("click", *canvas(x, y))],
		["mode 46", f"click {x} {y}"], fig)

ITEMS = [
	menu("open", "File", 52, "Open"),
	menu("merge", "File", 67, "Merge"),
	menu("save_as", "File", 97, "Save As"),
	menu("export", "File", 112, "Export"),
	menu("print", "File", 127, "Print"),
	menu("search_replace", "Edit", 83, "Search/Replace"),
	menu("global_settings", "Edit", 131, "Global settings"),
	menu("units", "Edit", 148, "Set units"),
	menu("styles", "View", 35, "Manage Styles"),
	menu("character_map", "View", 163, "Character map"),
	menu("about", "Help", 99, "About"),
	edit("box", 100, 150), edit("ellipse", 475, 150),
	edit("polyline", 200, 300), edit("arc", 450, 300),
	edit("spline", 100, 400), edit("text", 550, 412),
	edit("picture", 80, 200, FIG),
	("library", "Library mode", [("click", OFFX + 37, 322)], ["mode 17"], FIG),
] + [indicator("box", k) for k in range(len(INDICATORS["box"]))] + [
	indicator("text", k) for k in range(5, len(INDICATORS["text"]))] + [
	indicator("polyline", k) for k in (4, 12, 13, 14, 15)]


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


def old_dialog(env, steps, fig, png):
	"""Open the dialog in a fresh X11 xfig; return its window's names."""
	proc = subprocess.Popen([OLD_XFIG, "-geometry", f"1200x850+{OFFX}+0",
			str(fig)], env=env, cwd=fig.parent,
			stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
	try:
		for _ in range(60):
			time.sleep(0.25)
			if any(t.startswith("Xfig - ") for _, t in windows(env).values()):
				break
		time.sleep(1)
		before = set(windows(env))
		for step in steps:
			run([xclick()] + ([str(v) for v in step[1:]] if step[0] == "click"
				else [str(v) for v in step]), env=env)
			time.sleep(0.7)
		time.sleep(1)
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


def qt_dialog(steps, fig, png):
	with tempfile.TemporaryDirectory() as tmp:
		script = Path(tmp) / "s.txt"
		script.write_text(f"open {fig}\nwait 300\n" + "\n".join(steps) +
				f"\nwait 1000\nsnapwin {png}\nquit\n")
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
		for name, title, old_steps, qt_steps, fig in ITEMS:
			if args.names and not any(name.startswith(n) for n in args.names):
				continue
			for f in (out / f"{name}_old.png", out / f"{name}_qt.png"):
				f.unlink(missing_ok=True)
			old = old_dialog(env, old_steps, fig, out / f"{name}_old.png")
			qt = qt_dialog(qt_steps, fig, out / f"{name}_qt.png")
			print(f"{name:20} X11: {old[1] if old else 'none':30} "
			      f"Qt: {'ok' if qt else 'none'}", flush=True)
			rows.append((name, title, old))
	finally:
		xvfb.terminate()
		xvfb.wait()

	parts = ["<!doctype html><meta charset=utf-8><title>xfig dialogs</title>",
		 "<style>body{font:14px sans-serif;margin:16px}td{vertical-align:top;"
		 "padding:4px 12px 16px 0}img{border:1px solid #ccc;max-width:48vw}"
		 "</style><h1>xfig dialogs: X11 xfig (left), xfig-qt (right)</h1>"
		 "<table>"]
	for name, title, old in rows:
		parts.append(f"<tr><td colspan=2><b>{html.escape(title)}</b> "
			     f"{html.escape(old[0]) if old else ''}</td></tr><tr>"
			     f"<td><img src='{name}_old.png'></td>"
			     f"<td><img src='{name}_qt.png'></td></tr>")
	parts.append("</table>")
	(out / "index.html").write_text("\n".join(parts))
	print(f"report: {out / 'index.html'}")


if __name__ == "__main__":
	main()
