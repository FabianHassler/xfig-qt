#!/usr/bin/env python3
"""
compare.py - render .fig files in the original X11 xfig and in xfig-qt and
report the figures whose canvas renderings differ.

The X11 xfig runs on private Xvfb displays (one per job); xfig-qt runs with
the offscreen Qt platform and the snapcanvas test-script command.  Both
canvases are compared from their top-left corner, over the common size,
allowing for a one-pixel shift.

	tests/compare/compare.py [options] FILE_OR_DIR...

Output (default build/compare): old/, qt/ and diff/ PNGs, results.csv and
index.html, sorted worst first.  Exit status 1 if any figure scores above
--fail.
"""

import argparse
import concurrent.futures as cf
import csv
import html
import os
import queue
import random
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
OLD_XFIG = "/opt/homebrew/bin/xfig"
X11BIN = Path("/opt/X11/bin")
QT_XFIG = ROOT / "build/xfig.app/Contents/MacOS/xfig"

# xfig puts its popups (error messages, ...) at 0,0: keep them off the canvas
OLD_GEOMETRY = "1200x900+680+0"
OLD_CANVAS_BG = (247, 247, 247)	# Fig.canvasbackground of the X11 xfig
TOLERANCE = 60			# per-channel difference that counts
INK = 60			# darker than white by this much is "ink"


def run(cmd, env=None, timeout=60, **kw):
	return subprocess.run(cmd, env=env, timeout=timeout,
			stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, **kw)


# ------------------------------------------------------------ X11 xfig

class Display:
	def __init__(self, num):
		self.num = num
		self.name = f":{num}"
		self.env = dict(os.environ, DISPLAY=self.name)
		self.proc = subprocess.Popen(
			[X11BIN / "Xvfb", self.name, "-screen", "0", "1900x1000x24",
			 "-nolisten", "tcp"],
			stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
		for _ in range(50):
			if run([X11BIN / "xdpyinfo"], env=self.env).returncode == 0:
				return
			time.sleep(0.1)
		raise RuntimeError(f"Xvfb {self.name} did not start")

	def close(self):
		self.proc.terminate()
		self.proc.wait()

	def windows(self):
		out = run([X11BIN / "xwininfo", "-root", "-tree"], env=self.env).stdout
		return out.decode(errors="replace")

	def capture(self, win, png, tmp):
		xwd = Path(tmp) / "w.xwd"
		run([X11BIN / "xwd", "-id", win, "-silent", "-out", xwd], env=self.env)
		run(["ffmpeg", "-loglevel", "error", "-y", "-i", xwd,
		     "-pix_fmt", "rgb24", png])


def render_old(disp, fig, png, notes, timeout=20):
	"""Open fig in X11 xfig, wait until the canvas stops changing, save it."""
	title = f'"Xfig - {fig.name}"'
	proc = subprocess.Popen(
		[OLD_XFIG, "-geometry", OLD_GEOMETRY, str(fig)], env=disp.env,
		cwd=fig.parent, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
	try:
		with tempfile.TemporaryDirectory() as tmp:
			# large figures can hold still for a while during loading
			settle = max(0.0, fig.stat().st_size / 40e3 - 1)
			deadline = time.time() + timeout + settle
			canvas, last, same = None, None, 0
			while time.time() < deadline:
				time.sleep(0.3)
				if proc.poll() is not None:
					return "xfig exited"
				tree = disp.windows()
				if canvas is None:
					if title not in tree:
						continue
					# the canvas is the 1006x770 child of the main window
					m = re.search(r"(0x[0-9a-f]+) \(has no name\): \(\)\s+"
							r"1006x770\+", tree)
					if not m:
						continue
					canvas = m.group(1)
					time.sleep(settle)
				disp.capture(canvas, png, tmp)
				notes.discard("error popup")
				if '"Xfig: Error messages"' in tree:
					notes.add("error popup")
				# Xvfb shows unpainted areas black, and under load a
				# busy xfig can hold still for a moment before
				# drawing: require ~1s without change
				data = Path(png).read_bytes() if Path(png).exists() else None
				same = same + 1 if data and data == last else 0
				if same >= 3:
					return None
				last = data
			return "timeout"
	finally:
		proc.kill()
		proc.wait()


# -------------------------------------------------------------- xfig-qt

def render_qt(fig, png, timeout=30):
	with tempfile.TemporaryDirectory() as tmp:
		script = Path(tmp) / "script.txt"
		script.write_text(f"wait 300\nsnapcanvas {png}\nquit\n")
		env = dict(os.environ, QT_QPA_PLATFORM="offscreen",
				XFIGQT_SCRIPT=str(script))
		try:
			run([QT_XFIG, str(fig)], env=env, timeout=timeout,
					cwd=fig.parent)
		except subprocess.TimeoutExpired:
			return "timeout"
	return None if Path(png).exists() else "no snapshot"


# ----------------------------------------------------------- comparison

def load(png, bg=None):
	a = np.asarray(Image.open(png).convert("RGB")).astype(np.int16)
	if bg is not None:
		a = a.copy()
		a[(a == bg).all(axis=2)] = 255
	return a


def compare(old_png, qt_png, diff_png):
	"""Fraction of inked pixels that differ beyond a one-pixel shift."""
	old = load(old_png, OLD_CANVAS_BG)
	qt = load(qt_png)
	h = min(old.shape[0], qt.shape[0])
	w = min(old.shape[1], qt.shape[1])
	old, qt = old[:h, :w], qt[:h, :w]

	def shift_min(a, b):
		pb = np.pad(b, ((1, 1), (1, 1), (0, 0)), mode="edge")
		best = None
		for dy in (0, 1, 2):
			for dx in (0, 1, 2):
				d = np.abs(a - pb[dy:dy + h, dx:dx + w]).max(axis=2)
				best = d if best is None else np.minimum(best, d)
		return best

	# a pixel differs if neither image finds it in the other nearby
	bad = (shift_min(old, qt) > TOLERANCE) | (shift_min(qt, old) > TOLERANCE)
	ink = ((255 - old.min(axis=2)) > INK) | ((255 - qt.min(axis=2)) > INK)
	score = bad.sum() / max(ink.sum(), 1)

	gray = old.mean(axis=2, keepdims=True).repeat(3, axis=2)
	vis = (255 - (255 - gray) * 0.35).astype(np.uint8)
	vis[bad] = (230, 0, 0)
	Image.fromarray(vis).save(diff_png)
	return float(score), int(bad.sum()), int(ink.sum())


# ------------------------------------------------------------------ main

def find_figs(paths):
	figs = []
	for p in map(Path, paths):
		figs += sorted(p.rglob("*.fig")) if p.is_dir() else [p]
	return [f.resolve() for f in figs]


def key_of(fig, base):
	try:
		rel = fig.relative_to(base)
	except ValueError:
		rel = Path(fig.name)
	return str(rel.with_suffix("")).replace(os.sep, "__")


def write_report(out, rows):
	with open(out / "results.csv", "w", newline="") as f:
		w = csv.writer(f)
		w.writerow(["figure", "score", "bad_px", "ink_px", "error", "note"])
		for r in rows:
			w.writerow([r["fig"], f'{r["score"]:.4f}', r["bad"], r["ink"],
					r["error"] or "", r["note"]])
	parts = ["<!doctype html><meta charset=utf-8><title>xfig-qt compare</title>",
		"<style>body{font:14px sans-serif;margin:16px}"
		"img{max-width:32%;border:1px solid #ccc;vertical-align:top}"
		"h3{margin:24px 0 4px}</style>",
		f"<h1>xfig vs xfig-qt: {len(rows)} figures</h1>",
		"<p>Left: X11 xfig. Middle: xfig-qt. Right: differences in red."
		" Score = differing pixels / inked pixels.</p>"]
	for r in rows:
		k = html.escape(r["key"])
		parts.append(f'<h3>{html.escape(r["fig"])} &mdash; score '
				f'{r["score"]:.3f} {html.escape(r["error"] or "")}'
				f' <small>{html.escape(r["note"])}</small></h3>'
				f'<img src="old/{k}.png"><img src="qt/{k}.png">'
				f'<img src="diff/{k}.png">')
	(out / "index.html").write_text("\n".join(parts))


def main():
	ap = argparse.ArgumentParser(description=__doc__,
			formatter_class=argparse.RawDescriptionHelpFormatter)
	ap.add_argument("paths", nargs="+", help=".fig files or directories")
	ap.add_argument("-o", "--out", default=ROOT / "build/compare", type=Path)
	ap.add_argument("-j", "--jobs", default=4, type=int)
	ap.add_argument("--sample", type=int,
			help="compare a random sample of this many figures")
	ap.add_argument("--seed", type=int, default=1)
	ap.add_argument("--resume", action="store_true",
			help="keep the renderings of an interrupted run in --out"
			" and render only the missing figures")
	ap.add_argument("--qt", type=Path,
			help="xfig-qt binary (default: build/xfig.app)")
	ap.add_argument("--fail", type=float, default=0.05,
			help="score above which a figure counts as different")
	args = ap.parse_args()
	if args.qt:
		global QT_XFIG
		QT_XFIG = args.qt.resolve()

	figs = find_figs(args.paths)
	if args.sample and args.sample < len(figs):
		figs = sorted(random.Random(args.seed).sample(figs, args.sample))
	if not figs:
		sys.exit("no .fig files found")
	base = Path(os.path.commonpath([f.parent for f in figs]))

	out = args.out.resolve()
	for d in ("old", "qt", "diff"):
		if not args.resume:
			shutil.rmtree(out / d, ignore_errors=True)
		(out / d).mkdir(parents=True, exist_ok=True)

	displays = queue.Queue()
	# skip display numbers in use (another run, the user's X server)
	free = (n for n in range(140, 400)
			if not Path(f"/tmp/.X11-unix/X{n}").exists()
			and not Path(f"/tmp/.X{n}-lock").exists())
	opened = [Display(next(free)) for _ in range(args.jobs)]
	for d in opened:
		displays.put(d)

	def one(fig):
		k = key_of(fig, base)
		old_png, qt_png = out / "old" / f"{k}.png", out / "qt" / f"{k}.png"
		notes = set()
		# the diff image is written last: its figure was finished
		if args.resume and (out / "diff" / f"{k}.png").exists():
			err = None
		else:
			disp = displays.get()
			try:
				err = render_old(disp, fig, old_png, notes)
			finally:
				displays.put(disp)
			err = err or render_qt(fig, qt_png)
		row = dict(fig=str(fig.relative_to(base)), key=k, error=err,
				note=", ".join(sorted(notes)),
				score=1.0, bad=0, ink=0)
		if not err:
			row["score"], row["bad"], row["ink"] = compare(
					old_png, qt_png, out / "diff" / f"{k}.png")
		return row

	rows = []
	try:
		with cf.ThreadPoolExecutor(args.jobs) as ex:
			for i, row in enumerate(ex.map(one, figs), 1):
				rows.append(row)
				flag = "  DIFF" if row["score"] > args.fail else ""
				print(f'[{i}/{len(figs)}] {row["score"]:.3f} {row["fig"]}'
						f'{" " + row["error"] if row["error"] else ""}'
						f'{" (" + row["note"] + ")" if row["note"] else ""}{flag}',
						flush=True)
	finally:
		for d in opened:
			d.close()

	rows.sort(key=lambda r: -r["score"])
	write_report(out, rows)
	bad = [r for r in rows if r["score"] > args.fail]
	print(f"\n{len(bad)} of {len(rows)} figures differ (score > {args.fail}); "
			f"report: {out / 'index.html'}")
	sys.exit(1 if bad else 0)


if __name__ == "__main__":
	main()
