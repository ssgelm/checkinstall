#!/bin/bash
# --addso=yes has to find the libraries where they were staged, and the
# postinstall guard it writes has to work under /bin/sh (#892985).
. "$HARNESS"

d=$WORK/pkg; make_pkgdir "$d"
printf 'int f(void){return 1;}\n' > "$d/f.c"
gcc -shared -fPIC -o "$d/libckt.so.1" "$d/f.c" 2>/dev/null || { skip "no compiler for a shared library"; finish; }
cat > "$d/Makefile" <<'INNER'
install:
	install -d $(DESTDIR)/usr/local/lib
	install -m 644 libckt.so.1 $(DESTDIR)/usr/local/lib/
INNER

run_ck "$d" --addso=yes --pkgname=ckt-addso --pkgversion=1.0 make install >/dev/null 2>&1
deb=$PKGDIR/ckt-addso_1.0-1_*.deb
if ! ls $deb >/dev/null 2>&1; then fail "no package was built"; finish; fi

postinst=$(dpkg-deb --ctrl-tarfile $deb | tar -xO ./postinst 2>/dev/null)
if [ -z "$postinst" ]; then
	fail "no postinstall script was written, so the library was never found"
	finish
fi
ok "the staged library was recognised and a postinstall script written"

if printf '%s' "$postinst" | grep -q '//usr/local/lib'; then
	fail "the ld.so.conf entry has a doubled leading slash"
else
	ok "the ld.so.conf entry has a single leading slash"
fi

# the guard has to hold when the directory is already listed, under dash
conf=$WORK/ld.so.conf; printf '/usr/local/lib\n' > "$conf"
printf '%s' "$postinst" | sed -e "s|/etc/ld.so.conf|$conf|g" -e 's/^ldconfig$/:/' > "$WORK/postinst.sh"
sh "$WORK/postinst.sh" >/dev/null 2>&1
sh "$WORK/postinst.sh" >/dev/null 2>&1
if [ "$(wc -l < "$conf")" -eq 1 ]; then
	ok "the guard held: no duplicate entries after two installs"
else
	fail "the entry was appended again: $(tr '\n' ' ' < "$conf")"
fi
finish
