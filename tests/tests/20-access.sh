#!/bin/bash
# Only access() was wrapped. faccessat() and euidaccess() asked the real
# filesystem, so a file that exists only in the translated tree came back
# missing. rm checks write access before unlinking and gave up with ENOENT
# on hppa, where its rm takes that path and amd64's does not.
. "$HARNESS"

gcc -Wall -o "$WORK/access-ops" "$(dirname "$0")/../src/access-ops.c" 2>/dev/null || {
	fail "could not build access-ops"; finish; }

base=$WORK/root; mkdir -p "$base"
target=$base/only-in-transl

# One invocation: run_iw resets its root each time, so creating the file and
# asking about it have to happen inside the same run.
cat > "$WORK/ops.sh" <<INNER
#!/bin/sh
echo hello > "$target" || { echo "could not create it"; exit 1; }
"$WORK/access-ops" "$target" || exit 1
rm "$target" || { echo "rm failed"; exit 1; }
echo done
INNER
chmod +x "$WORK/ops.sh"

out=$(run_iw "$WORK/iw" 1 "$WORK/ops.sh" 2>&1)

if [ -e "$target" ]; then
	fail "the file reached the real filesystem, nothing was tested"
	rm -f "$target"
	finish
fi

case $out in
	*"ok"*done) ok "access, faccessat and euidaccess all see the translated file"
	            ok "rm removed a file that exists only in the translated tree" ;;
	*"rm failed"*) ok "access, faccessat and euidaccess all see the translated file"
	               fail "rm could not remove the translated file"; evidence 6 "$out" ;;
	*)          fail "an access check missed the translated file"
	            evidence 6 "$out" ;;
esac

# An access check on a descriptor rather than a name. The wrapper turned it
# into a name, which is a different question for a dangling symlink.
link=$base/dangling
ln -sfn /nonexistent-target "$link"
bare=$("$WORK/access-ops" "$target" "$link" 2>&1 | sed -n 's/^empty-path //p')
wrapped=$(run_iw "$WORK/iw2" 1 "$WORK/access-ops" "$target" "$link" 2>&1 |
          sed -n 's/^empty-path //p')
if [ -z "$bare" ] || [ -z "$wrapped" ]; then
	fail "the empty-path probe produced nothing (bare=[$bare] wrapped=[$wrapped])"
elif [ "$bare" = "$wrapped" ]; then
	ok "AT_EMPTY_PATH asks about the descriptor ($bare)"
else
	fail "AT_EMPTY_PATH: bare $bare, wrapped $wrapped"
fi

# Asking about a file must not drag it into the package.
probe=/usr/bin/env
run_iw "$WORK/iw3" 1 "$WORK/access-ops" "$probe" >/dev/null 2>&1
if [ -e "$WORK/iw3/TRANSL$probe" ]; then
	fail "access() copied $probe into the translated tree"
else
	ok "asking about a real file left it out of the translated tree"
fi

finish
