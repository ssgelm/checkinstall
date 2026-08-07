#!/bin/bash
# Temporary files are created inside the libc, out of reach of our open()
# wrapper, so mkstemp() and its relatives need wrapping themselves.
. "$HARNESS"

# 1. sed -i, the everyday case
d=$WORK/pkg; make_pkgdir "$d"
cat > "$d/sed.sh" <<'INNER'
#!/bin/sh
mkdir -p /usr/local/ckt
echo hello > /usr/local/ckt/conf
sed -i s/hello/world/ /usr/local/ckt/conf && grep -q world /usr/local/ckt/conf && echo "OK sed -i"
INNER
chmod +x "$d/sed.sh"
out=$(run_ck "$d" --pkgname=ckt-sed --pkgversion=1.0 ./sed.sh 2>&1)
if printf '%s' "$out" | grep -qF "OK sed -i"; then ok "sed -i under fstrans"; else fail "sed -i under fstrans"; fi

# 2. the LFS64 spellings, which is what mkstemps() becomes with
#    _FILE_OFFSET_BITS=64 and therefore what 32-bit builds really call
gcc -Wall -o "$WORK/lfs64-temps" "$(dirname "$0")/../src/lfs64-temps.c" 2>/dev/null || {
	fail "could not build lfs64-temps"; finish; }
t=$WORK/temps; mkdir -p "$t"
run_iw "$WORK/root-ts" 1 "$WORK/lfs64-temps" "$t" >/dev/null 2>&1
left=$(ls -A "$t")
made=$(find "$WORK/root-ts/TRANSL$t" -name '*.log' 2>/dev/null | wc -l)
if [ -n "$left" ]; then
	fail "mkstemps64/mkostemps64 wrote to the real filesystem: $left"
elif [ "$made" -ne 2 ]; then
	fail "mkstemps64/mkostemps64 did not land in the translated tree ($made of 2)"
else
	ok "mkstemps64 and mkostemps64 land in the translated tree"
fi
finish
