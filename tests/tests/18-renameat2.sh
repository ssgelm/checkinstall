#!/bin/bash
# renameat2's flags have to reach the kernel. RENAME_NOREPLACE only means
# anything if the test and the rename are the same operation, and dropping
# RENAME_EXCHANGE turns a swap into a one way rename, which destroys one of
# the two files.
. "$HARNESS"

gcc -Wall -o "$WORK/r2" "$(dirname "$0")/../src/renameat2-ops.c" 2>/dev/null || {
	skip "renameat2 unavailable on this system"; finish; }

base=$WORK/root; mkdir -p "$base"

# does this kernel and filesystem support the flags at all?
printf 'A\n' > "$base/p"; printf 'B\n' > "$base/q"
if ! "$WORK/r2" exchange "$base/p" "$base/q" >/dev/null 2>&1; then
	skip "RENAME_EXCHANGE unsupported here"
	finish
fi

# RENAME_NOREPLACE onto a name that exists must fail, and must fail the way
# a caller can detect: -1 with EEXIST, not a positive return
rm -rf "$base"; mkdir -p "$base"
printf 'AAA\n' > "$base/a"; printf 'BBB\n' > "$base/b"
out=$(run_iw "$WORK/iw1" 1 "$WORK/r2" noreplace "$base/a" "$base/b" 2>/dev/null)
case "$out" in
	"rc=-1 errno=17") ok "RENAME_NOREPLACE refused with EEXIST" ;;
	rc=0*)            fail "RENAME_NOREPLACE overwrote an existing name" ;;
	*)                fail "RENAME_NOREPLACE gave $out" ;;
esac

# RENAME_EXCHANGE must swap the two, not rename one over the other
rm -rf "$base"; mkdir -p "$base"
printf 'AAA\n' > "$base/a"; printf 'BBB\n' > "$base/b"
run_iw "$WORK/iw2" 1 "$WORK/r2" exchange "$base/a" "$base/b" >/dev/null 2>&1
ta=$WORK/iw2/TRANSL$base/a
tb=$WORK/iw2/TRANSL$base/b
if [ ! -e "$ta" ] || [ ! -e "$tb" ]; then
	fail "RENAME_EXCHANGE left one side missing in the translated tree"
elif [ "$(cat "$ta")" = "BBB" ] && [ "$(cat "$tb")" = "AAA" ]; then
	ok "RENAME_EXCHANGE swapped the two"
else
	fail "RENAME_EXCHANGE did not swap: a=$(cat "$ta") b=$(cat "$tb")"
fi

# and the real files are untouched throughout
if [ "$(cat "$base/a")" = "AAA" ] && [ "$(cat "$base/b")" = "BBB" ]; then
	ok "the real files were left alone"
else
	fail "the real files were modified"
fi

finish
