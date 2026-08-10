#!/bin/bash
# The rpm backend. It shares the file list and the installwatch logging with
# the deb backend and had no test of its own, so a change to the shared code
# could break it silently.
. "$HARNESS"

need_pkgtype R
need rpmbuild

d=$WORK/pkg; make_pkgdir "$d"
printf 'contents\n' > "$d/ckt.conf"
cat > "$d/Makefile" <<'INNER'
install:
	install -D -m 644 ckt.conf $(DESTDIR)/usr/share/ckt/ckt.conf
	install -d $(DESTDIR)/usr/share/ckt/emptydir
INNER

out=$(run_ck "$d" --pkgname=ckt-rpm --pkgversion=1.0 --pkgrelease=1 make install 2>&1)

pkg=$(find "$PKGDIR" -name 'ckt-rpm*.rpm' | head -1)
if [ -z "$pkg" ]; then
	fail "no rpm was built"
	evidence 15 "$out"
	finish
fi
ok "an rpm was built"

list=$(rpm -qlp "$pkg" 2>/dev/null)
if printf '%s\n' "$list" | grep -qx /usr/share/ckt/ckt.conf; then
	ok "the installed file is in the rpm"
else
	fail "the rpm does not list the installed file"
	evidence 10 "$list"
fi

# the metadata checkinstall was asked for has to reach the header
name=$(rpm -qp --qf '%{NAME}' "$pkg" 2>/dev/null)
ver=$(rpm -qp --qf '%{VERSION}' "$pkg" 2>/dev/null)
if [ "$name" = ckt-rpm ] && [ "$ver" = 1.0 ]; then
	ok "name and version reached the header"
else
	fail "header says name=$name version=$ver"
fi

finish
