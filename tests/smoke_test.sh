#!/bin/sh
# smoke_test.sh - run xfig-qt headless: find the data, open, save, snapshot
# and print a figure to PDF.  Used by CI on macOS, Linux and Windows.
#
#	tests/smoke_test.sh path/to/xfig [--no-print]
set -e

xfig=$1
here=$(cd "$(dirname "$0")/.." && pwd)
out=$(mktemp -d)

# on Windows (MSYS2), xfig-qt is a Windows program: give it Windows paths
native() {
	if command -v cygpath > /dev/null; then cygpath -m "$1"; else echo "$1"; fi
}
fig=$(native "$here/data/Libraries/Examples/pictures.fig")
o=$(native "$out")

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

# a Windows GUI program logs to the debugger, unless told otherwise
QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1 \
	XFIGQT_SCRIPT=$(native "$script") "$xfig" > "$out/log.txt" 2>&1 || true
cat "$out/log.txt"

fail=0
check() {
	if [ "$1" ]; then
		echo "ok:   $2"
	else
		echo "FAIL: $2"
		fail=1
	fi
}
check "$(grep 'datadir .*data' "$out/log.txt")" "data directory found"
check "$(test -s "$out/canvas.png" && echo y)" "canvas snapshot written"
check "$(head -c 4 "$out/saved.fig" 2>/dev/null | grep '#FIG')" "figure saved"
check "$(grep -c '^4 ' "$out/saved.fig" 2>/dev/null | grep -v '^0$')" "saved figure has its text objects"
if [ "$2" != --no-print ]; then
	check "$(head -c 4 "$out/printed.pdf" 2>/dev/null | grep '%PDF')" "printed to PDF"
fi
rm -rf "$out"
exit $fail
