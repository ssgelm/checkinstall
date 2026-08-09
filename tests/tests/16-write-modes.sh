#!/bin/bash
# Whether an open reaches the translation was decided from O_WRONLY and
# O_RDWR alone, and an fopen mode was searched for '+' only in the second
# character. Both let a call that modifies a file write to the real
# filesystem instead of the translated tree.
. "$HARNESS"

gcc -Wall -o "$WORK/write-modes" "$(dirname "$0")/../src/write-modes.c" 2>/dev/null || {
	fail "could not build write-modes"; finish; }

base=$WORK/root; mkdir -p "$base"

# O_RDONLY with O_CREAT still makes a file
target=$base/created
rm -f "$target"
run_iw "$WORK/iw1" 1 "$WORK/write-modes" rdonly-creat "$target" >/dev/null 2>&1
if [ -e "$target" ]; then
	fail "O_RDONLY|O_CREAT created a file on the real filesystem"
	rm -f "$target"
elif [ -e "$WORK/iw1/TRANSL$target" ]; then
	ok "O_RDONLY|O_CREAT was translated"
else
	fail "O_RDONLY|O_CREAT created nothing anywhere"
fi

# O_RDONLY with O_TRUNC still empties one
target=$base/victim-trunc
printf 'keep me\n' > "$target"
run_iw "$WORK/iw2" 1 "$WORK/write-modes" rdonly-trunc "$target" >/dev/null 2>&1
if [ ! -s "$target" ]; then
	fail "O_RDONLY|O_TRUNC emptied the real file"
elif [ ! -e "$WORK/iw2/TRANSL$target" ]; then
	fail "O_RDONLY|O_TRUNC did not truncate anything in the translated tree"
elif [ -s "$WORK/iw2/TRANSL$target" ]; then
	fail "the translated copy was not truncated, so nothing was tested"
else
	ok "O_RDONLY|O_TRUNC truncated the translated copy and left the real file"
fi

# glibc takes "rb+" as readily as "r+b"
target=$base/victim-fopen
printf 'original\n' > "$target"
run_iw "$WORK/iw3" 1 "$WORK/write-modes" fopen-rbplus "$target" >/dev/null 2>&1
if [ "$(cat "$target")" != "original" ]; then
	fail 'fopen "rb+" wrote through to the real file'
elif ! grep -q clobbered "$WORK/iw3/TRANSL$target" 2>/dev/null; then
	fail 'fopen "rb+" did not write into the translated copy either'
else
	ok 'fopen "rb+" wrote to the translated copy, not the real file'
fi

# and a genuine read must not be dragged into the translated tree
target=$base/readme
printf 'content\n' > "$target"
run_iw "$WORK/iw4" 1 "$WORK/write-modes" plain-read "$target" >/dev/null 2>&1
if [ -e "$WORK/iw4/TRANSL$target" ]; then
	fail "a plain O_RDONLY was copied into the translated tree"
else
	ok "a plain O_RDONLY was left as a read"
fi

finish
