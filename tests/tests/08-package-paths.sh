#!/bin/bash
# A package must not ship aliased paths: an install writing to
# /lib/systemd/system belongs in /usr/lib/systemd/system (DEP 17).
. "$HARNESS"

need_pkgtype D
need dpkg-deb "this one reads a .deb"

[ -L /lib ] || { skip "/lib is not a symlink on this system"; finish; }

d=$WORK/pkg; make_pkgdir "$d"
printf 'unit\n' > "$d/ckt.service"
cat > "$d/Makefile" <<'INNER'
install:
	install -d $(DESTDIR)/lib/systemd/system
	install -m 644 ckt.service $(DESTDIR)/lib/systemd/system/
INNER

run_ck "$d" --pkgname=ckt-paths --pkgversion=1.0 make install >/dev/null 2>&1
deb=$PKGDIR/ckt-paths_1.0-1_*.deb
if ! ls $deb >/dev/null 2>&1; then fail "no package was built"; finish; fi

contents=$(dpkg-deb -c $deb | awk '{print $6}')
if printf '%s\n' "$contents" | grep -qx "./usr/lib/systemd/system/ckt.service"; then
	ok "the unit is packaged under /usr/lib"
else
	fail "the unit is not packaged under /usr/lib"
	printf '%s\n' "$contents" | sed 's/^/      /'
fi
if printf '%s\n' "$contents" | grep -qE "^\./lib/?$|^\./lib/"; then
	fail "the package still contains the aliased ./lib path"
else
	ok "the package contains no aliased ./lib path"
fi
finish
