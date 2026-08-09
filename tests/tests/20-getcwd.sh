#!/bin/bash
# The getcwd() wrapper copied into the caller's buffer and then returned its
# own stack buffer instead. On hppa, where the stack grows upward, the
# caller read the frame back as pointer bytes and cmake was handed a path
# like /home/ssgelmAM-o^?TM-o^?Tl/tests/@*@*d.
. "$HARNESS"

gcc -Wall -o "$WORK/getcwd-ret" "$(dirname "$0")/../src/getcwd-ret.c" 2>/dev/null || {
	fail "could not build getcwd-ret"; finish; }

out=$(run_iw "$WORK/iw" 1 "$WORK/getcwd-ret" 2>&1)
if [ "$out" = ok ]; then
	ok "getcwd returned the caller's buffer"
else
	fail "getcwd under translation: $out"
fi

# and the same without translation, where the wrapper takes the other path
out=$(run_iw "$WORK/iw0" 0 "$WORK/getcwd-ret" 2>&1)
if [ "$out" = ok ]; then
	ok "getcwd returned the caller's buffer, untranslated"
else
	fail "getcwd untranslated: $out"
fi

finish
