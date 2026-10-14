#!/bin/sh
# smoke_test.sh - run xfig-qt headless: find the data, open, save, snapshot
# and print a figure to PDF, with Ghostscript and without (PDF and PNG
# export too).  Used by CI on macOS and Linux.
#
#	tests/smoke_test.sh path/to/xfig [--no-print]
set -e

xfig=$1
here=$(cd "$(dirname "$0")/.." && pwd)
out=$(mktemp -d)

fig=$here/data/Libraries/Examples/pictures.fig
o=$out

script=$out/script.txt
{
	echo "datadir"
	echo "open $fig"
	echo "wait 500"
	echo "snapcanvas $o/canvas.png"
	echo "saveas $o/saved.fig"
	[ "$2" = --no-print ] || echo "printpdf $o/printed.pdf"
	echo "quit"
} > "$script"

QT_QPA_PLATFORM=offscreen XFIGQT_SCRIPT=$script "$xfig" > "$out/log.txt" 2>&1 || true
cat "$out/log.txt"

# without Ghostscript: PDF and PNG are drawn from fig2dev's SVG
if [ "$2" != --no-print ]; then
	{
		echo "open $fig"
		echo "wait 500"
		echo "printpdf $o/printed_nogs.pdf"
		echo "export $o/exported_nogs.pdf 2"
		echo "export $o/exported_nogs.png 34"
		echo "quit"
	} > "$out/script_nogs.txt"
	XFIGQT_NO_GHOSTSCRIPT=1 QT_QPA_PLATFORM=offscreen \
		XFIGQT_SCRIPT=$out/script_nogs.txt "$xfig" > "$out/log_nogs.txt" 2>&1 || true
fi

fail=0
check() {
	if [ "$1" ]; then
		echo "ok:   $2"
	else
		echo "FAIL: $2"
		fail=1
	fi
}
datadir=$(sed -n 's/^datadir //p' "$out/log.txt" | head -n 1)
check "$(test -n "$datadir" && test -d "$datadir/Libraries" && echo y)" "data directory found"
check "$(test -s "$out/canvas.png" && echo y)" "canvas snapshot written"
check "$(head -c 4 "$out/saved.fig" 2>/dev/null | grep '#FIG')" "figure saved"
check "$(grep -c '^4 ' "$out/saved.fig" 2>/dev/null | grep -v '^0$')" "saved figure has its text objects"
if [ "$2" != --no-print ]; then
	check "$(head -c 4 "$out/printed.pdf" 2>/dev/null | grep '%PDF')" "printed to PDF"
	check "$(head -c 4 "$out/printed_nogs.pdf" 2>/dev/null | grep '%PDF')" "printed to PDF without Ghostscript"
	check "$(head -c 4 "$out/exported_nogs.pdf" 2>/dev/null | grep '%PDF')" "exported PDF without Ghostscript"
	check "$(head -c 4 "$out/exported_nogs.png" 2>/dev/null | tail -c 3 | grep 'PNG')" "exported PNG without Ghostscript"
fi
# SMOKE_OUT=dir keeps the snapshot, the saved figure, the PDFs and the PNG
if [ -n "$SMOKE_OUT" ]; then
	mkdir -p "$SMOKE_OUT"
	cp "$out"/canvas.png "$out"/saved.fig "$out"/*.pdf "$out"/*.png \
		"$SMOKE_OUT"/ 2>/dev/null || true
fi
rm -rf "$out"
exit $fail
