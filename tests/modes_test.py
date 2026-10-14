#!/usr/bin/env python3
"""
modes_test.py - use every drawing and editing mode of xfig-qt on a figure,
save it, and check what changed.

xfig-qt runs offscreen with a test script (ui/testscript.cpp): it opens
tests/compare/figs/objects.fig (or starts empty), selects a mode, clicks on
the canvas as a user would, and saves the figure.  The check compares the
objects in the saved file with the original.

	tests/modes_test.py [path/to/xfig] [-k name] [-v]
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
OBJECTS = HERE / "compare/figs/objects.fig"
DEFAULT_XFIG = (ROOT / "build/xfig.app/Contents/MacOS/xfig"
		if sys.platform == "darwin" else ROOT / "build/xfig")

# canvas positions on objects.fig (canvas pixels = fig units / 15)
BOX_EDGE = (100, 150)		# left edge of the box 100..250 x 100..200
BOX_CORNER = (250, 200)
ELLIPSE_EDGE = (325, 150)	# left of the ellipse, center (400,150)
LINE = (200, 300)		# the arrow line (100,300)..(300,300)
LINE_END = (300, 300)
ARC = (375, 225)		# top of the arc (450,300) (375,225) (300,300)
SPLINE_MID = (200, 450)		# the spline (100,400) (200,450) (300,400)
SPLINE_END = (300, 400)
TEXT = (520, 410)		# "Some text" at (500,420)

KINDS = {"0": "color", "1": "ellipse", "2": "polyline", "3": "spline",
	 "4": "text", "5": "arc", "6": "compound", "-6": "end"}


def objects(fig):
	"""The objects of a .fig file: a list of (kind, header line, points)."""
	objs = []
	lines = Path(fig).read_text(errors="replace").splitlines()
	body = [l for l in lines[9:] if not l.startswith("#")]
	for l in body:
		f = l.split()
		if not f:
			continue
		if not l[0].isspace() and f[0] in KINDS:
			objs.append([KINDS[f[0]], l, []])
		elif objs:
			objs[-1][2].append(l.strip())
	return [(k, h, " ".join(p)) for k, h, p in objs if k not in ("color",)]


def count(objs):
	return Counter(k for k, _, _ in objs if k != "end")


def click(p, button=1):
	return f"click {p[0]} {p[1]} {button}"


def move(p):
	return f"move {p[0]} {p[1]}"


def pick_move(src, dst):
	"""xfig's click, move, click."""
	return [click(src), move(dst), click(dst)]


# name: (start from objects.fig?, mode, script lines, check(orig, new))
def changed(o, n):
	return o != n


TESTS = {
	# drawing modes, on an empty figure
	"circle_radius": (False, 1, [click((200, 200)), move((250, 200)), click((250, 200))],
		lambda o, n: count(n)["ellipse"] == 1),
	"circle_diameter": (False, 2, [click((200, 200)), move((300, 200)), click((300, 200))],
		lambda o, n: count(n)["ellipse"] == 1),
	"ellipse": (False, 1, [click((200, 200)), move((300, 250)), click((300, 250))],
		lambda o, n: count(n)["ellipse"] == 1 and "1 1 " not in n[0][1][:4]),
	"picture": (False, 16, [click((100, 100)), move((300, 250)), click((300, 250)),
			"wait 800", "widget browse 0", "wait 800", "widget done 0"],
		lambda o, n: count(n)["polyline"] == 1 and "bugs.gif" in str(n)),
	"arc": (False, 5, [click((100, 300)), move((200, 200)), click((200, 200)),
			move((300, 300)), click((300, 300))],
		lambda o, n: count(n)["arc"] == 1),
	"polyline": (False, 6, [click((100, 100)), move((200, 150)), click((200, 150)),
			move((300, 100)), click((300, 100), 2)],
		lambda o, n: count(n)["polyline"] == 1),
	"box": (False, 7, [click((100, 100)), move((200, 200)), click((200, 200))],
		lambda o, n: count(n)["polyline"] == 1),
	"polygon": (False, 8, [click((100, 100)), move((200, 150)), click((200, 150)),
			move((150, 250)), click((150, 250), 2)],
		lambda o, n: count(n)["polyline"] == 1),
	"text": (False, 9, [click((200, 200)), "key Hello", "key \\r", click((300, 300))],
		lambda o, n: count(n)["text"] >= 1 and "Hello" in str(n)),
	"approx_spline": (False, 10, [click((100, 100)), move((200, 150)), click((200, 150)),
			move((300, 100)), click((300, 100), 2)],
		lambda o, n: count(n)["spline"] == 1),
	"closed_approx_spline": (False, 11, [click((100, 100)), move((200, 150)),
			click((200, 150)), move((150, 250)), click((150, 250), 2)],
		lambda o, n: count(n)["spline"] == 1),
	"interp_spline": (False, 12, [click((100, 100)), move((200, 150)), click((200, 150)),
			move((300, 100)), click((300, 100), 2)],
		lambda o, n: count(n)["spline"] == 1),
	"closed_interp_spline": (False, 13, [click((100, 100)), move((200, 150)),
			click((200, 150)), move((150, 250)), click((150, 250), 2)],
		lambda o, n: count(n)["spline"] == 1),
	"arcbox": (False, 14, [click((100, 100)), move((200, 200)), click((200, 200))],
		lambda o, n: count(n)["polyline"] == 1),
	"regpoly": (False, 15, [click((200, 200)), move((250, 200)), click((250, 200))],
		lambda o, n: count(n)["polyline"] == 1),

	# the keyboard shortcuts of the mode buttons (mode None: the key chooses)
	"key_b_box": (False, None, ["key b", "wait 200", click((100, 100)), move((200, 200)),
			click((200, 200))],
		lambda o, n: count(n)["polyline"] == 1),
	"key_B_arcbox": (False, None, ["key B", "wait 200", click((100, 100)), move((200, 200)),
			click((200, 200))],
		lambda o, n: count(n)["polyline"] == 1 and n[0][1].split()[1] == "4"),
	"key_c_circle": (False, None, ["key c", "wait 200", click((200, 200)), move((250, 200)),
			click((250, 200))],
		lambda o, n: count(n)["ellipse"] == 1),
	"key_l_polyline": (False, None, ["key l", "wait 200", click((100, 100)), move((200, 150)),
			click((200, 150), 2)],
		lambda o, n: count(n)["polyline"] == 1),
	"key_t_text": (False, None, ["key t", "wait 200", click((200, 200)), "key Hi", "key \\r"],
		lambda o, n: "Hi" in str(n)),
	"key_d_delete": (True, None, ["key d", "wait 200", click(BOX_EDGE)],
		lambda o, n: count(n)["polyline"] == count(o)["polyline"] - 1),
	"key_m_move": (True, None, ["key m", "wait 200"] + pick_move(BOX_EDGE, (100, 500)),
		lambda o, n: count(n) == count(o) and o != n),

	# editing modes, on objects.fig
	"glue": (True, 30, [click(BOX_EDGE), click(ELLIPSE_EDGE), click((600, 500), 3)],
		lambda o, n: count(n)["compound"] == 1),
	"scale": (True, 32, pick_move(BOX_CORNER, (300, 250)), changed),
	"copy": (True, 34, pick_move(BOX_EDGE, (100, 500)),
		lambda o, n: count(n)["polyline"] == count(o)["polyline"] + 1),
	"move": (True, 35, pick_move(BOX_EDGE, (100, 500)),
		lambda o, n: count(n) == count(o) and o != n),
	"delete": (True, 36, [click(BOX_EDGE)],
		lambda o, n: count(n)["polyline"] == count(o)["polyline"] - 1),
	"move_point": (True, 37, pick_move((100, 100), (50, 50)), changed),
	"delete_point": (True, 38, [click((200, 450))], changed),
	"add_point": (True, 39, pick_move(BOX_EDGE, (60, 150)), changed),
	"delete_arrow": (True, 40, [click(LINE_END)], changed),
	"add_arrow": (True, 41, [click(SPLINE_END)], changed),
	"flip": (True, 42, [click(BOX_EDGE)], changed),
	"rotate": (True, 43, [click(BOX_EDGE)], changed),
	"autoarrow": (True, 44, [click(SPLINE_MID)], changed),
	"convert": (True, 45, [click(SPLINE_MID)],	# spline -> polyline
		lambda o, n: count(n)["polyline"] == count(o)["polyline"] + 1),
	"convert_2_points": (True, 45, [click(LINE)],	# "Not enough points"
		lambda o, n: count(n) == count(o)),
	"split": (True, 56, [click(LINE, 2)],
		lambda o, n: count(n)["polyline"] == count(o)["polyline"] + 1),
	"chop": (True, 62, [click(LINE), click(ELLIPSE_EDGE, 2), click((600, 500), 3)],
		changed),
	"tangent": (True, 57, [click(ELLIPSE_EDGE)],
		lambda o, n: count(n)["polyline"] == count(o)["polyline"] + 1),
	"measure_angle": (True, 58, [click((100, 100)), move((250, 100)), click((250, 100)),
			move((100, 200)), click((100, 200))], lambda o, n: count(n) == count(o)),
	"measure_length": (True, 59, [click(LINE)], lambda o, n: count(n) == count(o)),
	"measure_area": (True, 60, [click(BOX_EDGE)], lambda o, n: count(n) == count(o)),
	"align": (True, 30, [click(BOX_EDGE), click(ELLIPSE_EDGE), click((600, 500), 3),
			"mode 48", "wait 200", click(BOX_EDGE)],
		lambda o, n: count(n)["compound"] == 1),
	"open_compound": (True, 30, [click(BOX_EDGE), click(ELLIPSE_EDGE),
			click((600, 500), 3), "mode 51", "wait 200", click(BOX_EDGE),
			"wait 300", move((180, 160)), click((180, 160)), "wait 200",
			"widget close_compound 0"],
		lambda o, n: count(n)["compound"] == 1),
	"update": (True, 47, [click(TEXT)], changed),
	"open_close": (True, 54, [click(LINE)], changed),
	"break": (True, 31, [click(BOX_EDGE)],	# no compound: nothing
		lambda o, n: count(n) == count(o)),
}


def run(xfig, name, test, tmp, verbose):
	start, mode, lines, check = test
	out = Path(tmp) / f"{name}.fig"
	script = Path(tmp) / f"{name}.txt"
	pre = [f"open {OBJECTS}", "wait 400"] if start else ["wait 300"]
	choose = [f"mode {mode}", "wait 200"] if mode is not None else []
	script.write_text("\n".join(pre + choose + ["enter"] +
			lines + ["wait 300", f"saveas {out}", "quit"]) + "\n")
	env = dict(os.environ, QT_QPA_PLATFORM="offscreen", XFIGQT_SCRIPT=str(script),
			HOME=tmp, XFIGQT_TEST_BROWSE=str(ROOT /
				"data/Libraries/Examples/bugs.gif"))
	try:
		p = subprocess.run([str(xfig)], env=env, capture_output=True, timeout=60)
		log = p.stderr.decode(errors="replace")
	except subprocess.TimeoutExpired:
		return name, False, "timeout"
	if not out.exists():
		return name, False, "not saved\n" + log[-500:]
	orig = objects(OBJECTS) if start else []
	new = objects(out)
	ok = bool(check(orig, new))
	detail = f"{dict(count(orig))} -> {dict(count(new))}"
	if verbose or not ok:
		detail += "\n" + "\n".join(f"    {k}: {h} | {pts}" for k, h, pts in new)
	return name, ok, detail


def main():
	ap = argparse.ArgumentParser(description=__doc__,
			formatter_class=argparse.RawDescriptionHelpFormatter)
	ap.add_argument("xfig", nargs="?", default=str(DEFAULT_XFIG))
	ap.add_argument("-k", action="append", help="only tests containing this")
	ap.add_argument("-v", action="store_true", help="show the saved objects")
	args = ap.parse_args()
	tests = {n: t for n, t in TESTS.items()
		 if not args.k or any(k in n for k in args.k)}
	with tempfile.TemporaryDirectory() as tmp:
		with ThreadPoolExecutor(4) as ex:
			results = list(ex.map(lambda nt: run(args.xfig, nt[0], nt[1], tmp, args.v),
					tests.items()))
	fails = 0
	for name, ok, detail in results:
		print(f"{'ok  ' if ok else 'FAIL'} {name:22} {detail}")
		fails += not ok
	print(f"{len(results) - fails} of {len(results)} modes work")
	sys.exit(1 if fails else 0)


if __name__ == "__main__":
	main()
