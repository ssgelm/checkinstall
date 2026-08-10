#!/bin/bash
# The Slackware backend, which builds a tarball through makepkg. Same
# reasoning as the rpm test: shared code, no coverage of its own until now.
. "$HARNESS"

need_pkgtype S
need "$CK_PKGTOOL" "no makepkg or makepak to build with"

d=$WORK/pkg; make_pkgdir "$d"
printf 'contents\n' > "$d/ckt.conf"
cat > "$d/Makefile" <<'INNER'
install:
	install -D -m 644 ckt.conf $(DESTDIR)/usr/share/ckt/ckt.conf
INNER

out=$(run_ck "$d" --pkgname=ckt-slack --pkgversion=1.0 make install 2>&1)

pkg=$(find "$PKGDIR" -name 'ckt-slack*.t[gx]z' 2>/dev/null | head -1)
if [ -z "$pkg" ]; then
	fail "no Slackware package was built"
	evidence 15 "$out"
	finish
fi
ok "a Slackware package was built"

if tar tf "$pkg" 2>/dev/null | grep -q 'usr/share/ckt/ckt.conf'; then
	ok "the installed file is in the tarball"
else
	fail "the tarball does not hold the installed file"
	evidence 10 "$(tar tf "$pkg" 2>&1)"
fi

  # Slackware packages carry their description under install/
if tar tf "$pkg" 2>/dev/null | grep -q 'install/slack-desc'; then
	ok "the package carries a slack-desc"
else
	skip "no install/slack-desc, which older makepkg does not add"
fi

finish
