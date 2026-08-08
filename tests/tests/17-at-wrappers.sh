#!/bin/bash
# The *at calls, driven with a real directory descriptor and a relative
# name while the working directory is somewhere else. A wrapper that
# resolved the name against the working directory would land in the decoy.
#
# This covers the ordinary translated path. The passthrough these wrappers
# take when wrapping is switched off is not reachable from here: the only
# window with INSTW_OKWRAP clear is inside canonicalize(), and nothing there
# makes a relative *at call.
. "$HARNESS"

gcc -Wall -o "$WORK/at-ops" "$(dirname "$0")/../src/at-ops.c" 2>/dev/null || {
	fail "could not build at-ops"; finish; }

base=$WORK/root
mkdir -p "$base/target" "$base/decoy"

# run from the decoy, act on the target through its descriptor
at() { ( cd "$base/decoy" && run_iw "$1" 1 "$WORK/at-ops" "$2" "$base/target" "${@:3}" ); }

landed() { # <iwroot> <name> <what>
	local iw=$1 name=$2 what=$3
	  # -e is false for a dangling symlink, so test -L as well
	if [ -e "$iw/TRANSL$base/target/$name" ] || [ -L "$iw/TRANSL$base/target/$name" ]; then
		ok "$what"
	elif [ -e "$iw/TRANSL$base/decoy/$name" ] || [ -L "$iw/TRANSL$base/decoy/$name" ] ||
	     [ -e "$base/decoy/$name" ] || [ -L "$base/decoy/$name" ]; then
		fail "$what (resolved against the working directory)"
	else
		fail "$what (did not appear anywhere)"
	fi
}

at "$WORK/iw-open" openat file.txt >/dev/null 2>&1
landed "$WORK/iw-open" file.txt "openat created the file under its descriptor"

at "$WORK/iw-mkdir" mkdirat subdir >/dev/null 2>&1
landed "$WORK/iw-mkdir" subdir "mkdirat created the directory under its descriptor"

at "$WORK/iw-sym" symlinkat link >/dev/null 2>&1
landed "$WORK/iw-sym" link "symlinkat created the link under its descriptor"

# fstatat has to see a file that is really there
printf 'x\n' > "$base/target/seen"
if at "$WORK/iw-stat" fstatat seen >/dev/null 2>&1; then
	ok "fstatat found a file through its descriptor"
else
	fail "fstatat did not find a file through its descriptor"
fi

# renameat, both ends on the same descriptor
printf 'x\n' > "$base/target/before"
at "$WORK/iw-ren" renameat before after >/dev/null 2>&1
landed "$WORK/iw-ren" after "renameat moved the file under its descriptor"

# unlinkat should remove inside the translated tree, not the real one
printf 'x\n' > "$base/target/doomed"
at "$WORK/iw-unlink" unlinkat doomed >/dev/null 2>&1
if [ -e "$base/target/doomed" ]; then
	ok "unlinkat left the real file in place"
else
	fail "unlinkat removed the real file"
fi

finish
