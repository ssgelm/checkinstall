#!/bin/bash
# Option values reach the package as given, shell metacharacters and all
# (#785441).
. "$HARNESS"

d=$WORK/pkg; make_pkgdir "$d"
printf 'hello\n' > "$d/hello.txt"
cat > "$d/Makefile" <<'INNER'
install:
	install -d $(DESTDIR)/usr/local/share/ckt
	install -m 644 hello.txt $(DESTDIR)/usr/local/share/ckt/
INNER

run_ck "$d" --pkgname=ckt-meta --pkgversion=1.0 \
	--maintainer="Jane Doe <jane@example.org>" \
	--requires="libc6 (>= 2.28), zlib1g" \
	--summary="A test package (100% synthetic)" \
	make install >/dev/null 2>&1
deb=$PKGDIR/ckt-meta_1.0-1_*.deb
if ! ls $deb >/dev/null 2>&1; then fail "no package was built"; finish; fi

check() { # check <field> <expected>; only the first line of Description
	got=$(dpkg-deb -f $deb "$1" | head -1)
	if [ "$got" = "$2" ]; then ok "$1 survived intact"
	else fail "$1: expected [$2], got [$got]"; fi
}
check Maintainer "Jane Doe <jane@example.org>"
check Depends "libc6 (>= 2.28), zlib1g"
check Description "A test package (100% synthetic)"
finish
