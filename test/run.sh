#!/usr/bin/env bash
# runs every case in test/cases and compares what it printed with the .out file
# that sits next to it.
#
#   ./test/run.sh ./bin/main            check every case
#   ./test/run.sh ./bin/main --bless    write the .out files from this run
#   ./test/run.sh ./bin/main 04_flow    only the cases whose name holds 04_flow
#
# a case can ask for options of its own, and can say what code it hands back to
# the shell, with two small files that sit next to it:
#
#   NAME.args   one line of extra options for abas, say '--function-depth 32'
#   NAME.exit   the code the case is expected to stop with, 0 when it is not there
#
# a case that does not stop with the code its .exit file names is a failure, and
# so is one whose output is not what its .out file says. what the runs printed
# goes to test/failures/<name>.out together with the difference.

set -u

BAS=${1:?usage: run.sh ./bin/main [--bless] [name ...]}
shift || true
BLESS=0
if [ "${1:-}" = "--bless" ]; then
	BLESS=1
	shift
fi

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")
CASES="$HERE/cases"
FAILS="$HERE/failures"
INPUT="q"

# the binary is found before the move, and every case is then named relative to
# the root, so that an error message holding a file name is the same whichever
# directory run.sh was called from
BAS=$(cd "$(dirname "$BAS")" && pwd)/$(basename "$BAS")
cd "$ROOT" || exit 1

mkdir -p "$FAILS"
rm -f "$FAILS"/*.out "$FAILS"/*.diff

want() {
	[ $# -eq 0 ] && return 0
	local name=$1
	shift
	[ $# -eq 0 ] && return 0
	local pick
	for pick in "$@"; do
		case "$name" in
			*"$pick"*) return 0 ;;
		esac
	done
	return 1
}

TOTAL=0
PASSED=0
FAILED=0
FAILED_NAMES=""

for file in "$CASES"/*.abas; do
	name=$(basename "$file" .abas)
	want "$name" "$@" || continue
	TOTAL=$((TOTAL + 1))

	rel="test/cases/$name.abas"
	got="$FAILS/$name.out"

	# the options of this case, if it has any. the second -I is the project root,
	# where lib/ lives
	args=()
	if [ -f "$CASES/$name.args" ]; then
		# shellcheck disable=SC2207
		args=($(cat "$CASES/$name.args"))
	fi

	# stdin is fed one character so that a case that reads input has something.
	"$BAS" -I "$HERE" -I "$ROOT" "${args[@]}" "$rel" >"$got" 2>&1 <<<"$INPUT"
	code=$?

	want_code=0
	if [ -f "$CASES/$name.exit" ]; then
		want_code=$(cat "$CASES/$name.exit")
	fi

	# stderr goes to the same place, so a run that failed has the message in it
	if [ "$code" -ne "$want_code" ]; then
		echo "FAIL $name: the program stopped with code $code, $want_code was expected"
		sed 's/^/     /' "$got"
		FAILED=$((FAILED + 1))
		FAILED_NAMES="$FAILED_NAMES $name"
		continue
	fi

	want_out=$CASES/$name.out
	if [ $BLESS -eq 1 ]; then
		mv "$got" "$want_out"
		echo "BLESS $name"
		PASSED=$((PASSED + 1))
		continue
	fi

	if [ ! -f "$want_out" ]; then
		echo "FAIL $name: there is no $(basename "$want_out") yet, run with --bless"
		FAILED=$((FAILED + 1))
		FAILED_NAMES="$FAILED_NAMES $name"
		continue
	fi

	if diff -u "$want_out" "$got" >"$FAILS/$name.diff" 2>&1; then
		# the run matched, so what it printed is of no use to anyone and the
		# two files that were made are taken away again
		rm -f "$FAILS/$name.diff" "$got"
		echo "ok   $name"
		PASSED=$((PASSED + 1))
		continue
	fi

	echo "FAIL $name: the output is not what $name.out says"
	sed 's/^/     /' "$FAILS/$name.diff"
	FAILED=$((FAILED + 1))
	FAILED_NAMES="$FAILED_NAMES $name"
done

echo
echo "$PASSED of $TOTAL case(s) passed"
if [ $FAILED -ne 0 ]; then
	echo "failed:$FAILED_NAMES"
	echo "what the runs printed is in $FAILS"
	exit 1
fi
# a run that passed leaves nothing behind, and rmdir says so rather than making
# a mess of it if something is in there
rmdir "$FAILS" 2>/dev/null
exit 0
