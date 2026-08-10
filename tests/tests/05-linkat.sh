#!/bin/bash
# linkat() must link inside the translated tree, and must keep working when
# the source is a descriptor rather than a path.
. "$HARNESS"

need "$CK_PKGTOOL" "no tool for $CK_PKGTYPE packages"

# 1. a hard link between two paths that exist for real must not reach /
d=$WORK/pkg; make_pkgdir "$d"
real=$WORK/real; mkdir -p "$real"; echo source > "$real/source"
cat > "$d/link.sh" <<INNER
#!/bin/sh
ln $real/source $real/escaped-hardlink
INNER
chmod +x "$d/link.sh"
run_ck "$d" --pkgname=ckt-link --pkgversion=1.0 ./link.sh >/dev/null 2>&1
if [ -e "$real/escaped-hardlink" ]; then
	fail "linkat escaped the translated tree and wrote to the real filesystem"
else
	ok "linkat left the real filesystem alone"
fi

# 2. AT_EMPTY_PATH: the source is an O_TMPFILE descriptor with no name
gcc -Wall -o "$WORK/tmpfile-link" "$(dirname "$0")/../src/tmpfile-link.c" 2>/dev/null || {
	fail "could not build tmpfile-link"; finish; }
# linkat(AT_EMPTY_PATH) wants CAP_DAC_READ_SEARCH and returns ENOENT without
# it, so an unprivileged run cannot do this at all on some kernels. Find out
# whether this machine permits it before reading a failure as a translation
# bug: run the helper once with nothing interposed.
"$WORK/tmpfile-link" "$WORK" "$WORK/probe" >/dev/null 2>&1
probe=$?
rm -f "$WORK/probe"

target=$WORK/materialised
run_iw "$WORK/root-tf" 1 "$WORK/tmpfile-link" "$WORK" "$target"
rc=$?
transl=$WORK/root-tf/TRANSL$target
if [ "$probe" -eq 77 ]; then
	skip "O_TMPFILE unsupported on this filesystem"
elif [ "$probe" -ne 0 ]; then
	skip "linkat(AT_EMPTY_PATH) not permitted here, nothing to translate"
elif [ "$rc" -ne 0 ]; then
	fail "linkat(AT_EMPTY_PATH) failed under translation"
elif [ -e "$target" ]; then
	fail "linkat(AT_EMPTY_PATH) wrote to the real filesystem"
elif [ ! -s "$transl" ]; then
	fail "linkat(AT_EMPTY_PATH) did not materialise the file in the translated tree"
else
	ok "linkat(AT_EMPTY_PATH) materialised an O_TMPFILE into the translated tree"
fi
finish
