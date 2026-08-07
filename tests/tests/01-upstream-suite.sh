#!/bin/bash
# installwatch's own wrapper tests, in both translation modes.
. "$HARNESS"

[ -n "${CK_SRC:-}" ] || { skip "no source tree: test-installwatch.c unavailable"; finish; }

# test-installwatch.c needs localdecls.h, which the build generates. In
# package mode nothing has been built, so make one here.
if [ -f "$CK_SRC/installwatch/localdecls.h" ]; then
	incdir=$CK_SRC/installwatch
else
	incdir=$WORK
	# create-localdecls compiles two helpers that sit beside it
	cp "$CK_SRC/installwatch/create-localdecls" \
	   "$CK_SRC/installwatch/libctest.c" \
	   "$CK_SRC/installwatch/libcfiletest.c" "$WORK/"
	( cd "$WORK" && ./create-localdecls ) >/dev/null 2>&1
fi

gcc -Wall -DVERSION='"test"' -DLIBDIR="\"$(dirname "$IW_SO")\"" \
    -o "$WORK/test-installwatch" "$CK_SRC/installwatch/test-installwatch.c" \
    -I"$incdir" -ldl 2>"$WORK/cc.log" || {
	# The test program only compiles once "Fix the installwatch test
	# program" is in, and package mode reads whatever tree it is given.
	if [ "${CK_MODE:-src}" = deb ]; then
		skip "test-installwatch.c does not compile from this source tree"
		skip "(needs a tree with the test-program fix in it)"
	else
		fail "could not build test-installwatch"
		sed 's/^/      /' "$WORK/cc.log"
	fi
	finish; }

for mode in 0 1; do
	out=$(run_iw "$WORK/root$mode" "$mode" "$WORK/test-installwatch" 2>&1)
	if printf '%s' "$out" | grep -q "All tests successful"; then
		ok "upstream suite, fstrans=$mode"
	else
		fail "upstream suite, fstrans=$mode"
		printf '%s\n' "$out" | grep -i "fail" | sed 's/^/      /'
	fi
done
finish
