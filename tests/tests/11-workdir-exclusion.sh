#!/bin/bash
# #1033483: files created in the build directory are found with an anchored
# grep and dropped with an unanchored one, so an installed path that merely
# contains the build directory's name was dropped too.
#
# This one is a source check rather than a run. Reaching the branch needs a
# build whose own files land in checkinstall's file list, which does not
# happen from this harness, so there is nothing to drive end to end. Replace
# it with a real run if you find a layout that gets there.
. "$HARNESS"

if grep -q 'grep -v "\^`pwd`"' "$CK"; then
	ok "the build directory exclusion is anchored"
else
	fail "the build directory exclusion is not anchored"
	grep -n 'grep -v "`pwd`"' "$CK" | sed 's/^/      /'
fi
finish
