#!/bin/bash
# An excluded path is not translated, so anything written under one reaches
# the real filesystem. The match therefore has to stop at a component
# boundary: /tmp must not exclude /tmpfoo, and /usr/local must not exclude
# /usr/locality.
. "$HARNESS"

base=$WORK/root
mkdir -p "$base/excl" "$base/exclfoo" "$base/deep/local/lib" "$base/deep/locality"

# what installwatch is told to leave alone
export INSTW_EXCLUDE="$base/excl,$base/deep/local,"

check() { # <path> <expect: real|translated> <what>
	local path=$1 expect=$2 what=$3
	rm -f "$path"
	# an ordinary write, so a failure here is about exclusions and not
	# about which open flags reach the translation
	run_iw "$WORK/iw" 1 "$WORK/write-modes" wronly-creat "$path" >/dev/null 2>&1
	local got
	if [ -e "$WORK/iw/TRANSL$path" ]; then got=translated
	elif [ -e "$path" ]; then got=real
	else got=neither; fi
	if [ "$got" = "$expect" ]; then
		ok "$what"
	else
		fail "$what (expected $expect, got $got)"
	fi
	rm -f "$path"
}

gcc -Wall -o "$WORK/write-modes" "$(dirname "$0")/../src/write-modes.c" 2>/dev/null || {
	fail "could not build write-modes"; finish; }

# inside an exclusion: must reach the real filesystem, that being the point
check "$base/excl/inside"        real       "a path under an exclusion is left alone"
check "$base/deep/local/lib/x"   real       "a path under a nested exclusion is left alone"

# merely sharing a prefix: must still be translated
check "$base/exclfoo/x"          translated "a sibling sharing the prefix is still translated"
check "$base/deep/locality/x"    translated "a longer name sharing the prefix is still translated"

# and something unrelated
check "$base/other"              translated "an unrelated path is translated"

finish
