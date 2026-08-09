#!/bin/bash
# A mode set on a symlink must not reach the file it points at. tar asks for
# that with AT_SYMLINK_NOFOLLOW on every symlink it extracts. When the flag
# was dropped the mode went to the target, so a 755 binary came out of the
# package writable by anyone, and a link extracted before its target aborted
# the install outright.
#
# This is also the integration case for 19. Where glibc is older than 2.32
# and refuses AT_SYMLINK_NOFOLLOW, tar falls back to an O_PATH descriptor
# and AT_EMPTY_PATH, so a wrapper that rebuilds a pathname for the empty one
# breaks the extraction here.
. "$HARNESS"

# The symlink sorts before its target, so tar extracts it while it dangles.
d=$WORK/sym; make_pkgdir "$d"
mkdir -p "$d/stage/usr/local/bin"
printf '#!/bin/sh\necho hi\n' > "$d/stage/usr/local/bin/target"
chmod 755 "$d/stage/usr/local/bin/target"
ln -sf target "$d/stage/usr/local/bin/aaa-link"
tar cf "$d/pkg.tar" -C "$d/stage" usr

{ printf 'install:\n'
  printf '\ttar -C $(DESTDIR)/ -xf pkg.tar\n'
} > "$d/Makefile"

run_ck "$d" --pkgname=ckt-symmode --pkgversion=1.0 make install >"$WORK/l.log" 2>&1
deb=$(ls $PKGDIR/ckt-symmode_1.0-1_*.deb 2>/dev/null | head -1)
if [ -z "$deb" ]; then
	fail "the install aborted, no package built"
	grep -iE "cannot change mode|Exiting with failure|Installation failed" "$WORK/l.log" \
		| head -4 | sed 's/^/      /'
	finish
fi
ok "a link extracted before its target no longer aborts the install"

listing=$(dpkg-deb -c "$deb")

mode=$(printf '%s\n' "$listing" | awk '$6 ~ /bin\/target$/ {print $1}')
case "$mode" in
	-rwxr-xr-x) ok "the target keeps its own 755" ;;
	*w?w*|*w??w*|*ww*) fail "the target became group or world writable ($mode)" ;;
	"")  fail "the target is not in the package"
	     printf '%s\n' "$listing" | sed 's/^/      /' ;;
	*)   fail "the target has an unexpected mode ($mode)" ;;
esac

if printf '%s\n' "$listing" | grep -q "bin/aaa-link ->"; then
	ok "the link is packaged as a link"
else
	fail "the link was not packaged as a link"
	printf '%s\n' "$listing" | sed 's/^/      /'
fi

# The real filesystem must be untouched either way
if [ -e /usr/local/bin/target ] || [ -e /usr/local/bin/aaa-link ]; then
	fail "something escaped into /usr/local/bin on this machine"
else
	ok "nothing was written outside the translated tree"
fi

finish
