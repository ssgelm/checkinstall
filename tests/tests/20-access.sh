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

finish
