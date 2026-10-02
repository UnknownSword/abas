#!/usr/bin/env bash
# runs every lesson in totor/ and fails if any of them stops with an error.
#
#   ./totor/run.sh              check every lesson
#   ./totor/run.sh 13           only the lessons whose name holds 13
#
# 9_io and 12_stdlib read stdin, so they are given a line to read.

set -u

HERE=$(dirname "$0")
ROOT=$(dirname "$HERE")
BAS="$ROOT/bin/main"

[ -x "$BAS" ] || { echo "error: build it first with 'make build'"; exit 1; }
[ $# -eq 0 ] || echo "running the lessons that hold: $*"

INPUT=$'hello there\n'
TOTAL=0
PASSED=0
FAILED=0
FAILED_NAMES=""

for file in "$HERE"/*.abas; do
	name=$(basename "$file" .abas)
	[ $# -eq 0 ] || case "$name" in
		*"$@"*) ;;
		*) continue ;;
	esac
	TOTAL=$((TOTAL + 1))

	if printf '%s' "$INPUT" | "$BAS" -I "$ROOT" "$file" >/dev/null 2>"$HERE/$name.err"; then
		rm -f "$HERE/$name.err"
		echo "ok   $name"
		PASSED=$((PASSED + 1))
		continue
	fi

	echo "FAIL $name"
	sed 's/^/     /' "$HERE/$name.err"
	FAILED=$((FAILED + 1))
	FAILED_NAMES="$FAILED_NAMES $name"
done

echo
echo "$PASSED of $TOTAL lesson(s) ran to the end"
if [ $FAILED -ne 0 ]; then
	echo "failed:$FAILED_NAMES"
	exit 1
fi
exit 0
