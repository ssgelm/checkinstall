#!/bin/bash
# fstatat with AT_EMPTY_PATH must keep descriptor semantics. Rebuilding a
# pathname from /proc/self/fd turns a question about an O_PATH descriptor
# into a question about a name, and for a dangling symbolic link the two
# have different answers.
. "$HARNESS"

gcc -Wall -o "$WORK/empty-path" "$(dirname "$0")/../src/empty-path.c" 2>/dev/null || {
	fail "could not build empty-path"; finish; }

mkdir -p "$WORK/bare" "$WORK/wrapped"

( cd "$WORK/bare" && "$WORK/empty-path" ); bare=$?
if [ "$bare" -eq 77 ]; then
	skip "O_PATH unsupported on this system"
	finish
elif [ "$bare" -ne 0 ]; then
	fail "the bare system cannot do this, so there is nothing to compare against"
	finish
fi
ok "bare: AT_EMPTY_PATH reports the link"

( cd "$WORK/wrapped" && run_iw "$WORK/iw" 1 "$WORK/empty-path" )
if [ $? -eq 0 ]; then
	ok "wrapped: AT_EMPTY_PATH reports the link too"
else
	fail "wrapped: AT_EMPTY_PATH did not report the link"
fi

finish
