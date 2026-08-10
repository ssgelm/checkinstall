#!/bin/bash
# Paths with spaces in them. The file list used to be carried around as
# space separated words, so a name with two spaces in it came back with one
# and the file was dropped from the package.
. "$HARNESS"

need_pkgtype D
need dpkg-deb "this one reads a .deb"

# ---- a file whose name contains two consecutive spaces ----
d=$WORK/dbl; make_pkgdir "$d"
{ printf 'install:\n'
  printf '\tinstall -d $(DESTDIR)/usr/local/sp\n'
  printf '\tprintf x > "$(DESTDIR)/usr/local/sp/Hello  World"\n'
  printf '\tprintf x > "$(DESTDIR)/usr/local/sp/Hello World"\n'
  printf '\tprintf x > "$(DESTDIR)/usr/local/sp/trailing "\n'
} > "$d/Makefile"
run_ck "$d" --pkgname=ckt-spaces --pkgversion=1.0 make install >"$WORK/d.log" 2>&1
deb=$(ls $PKGDIR/ckt-spaces_1.0-1_*.deb 2>/dev/null | head -1)
if [ -z "$deb" ]; then
	fail "no package was built"
	tail -15 "$WORK/d.log" | sed 's/^/      /'
else
	c=$(dpkg-deb -c "$deb")
	printf '%s' "$c" | grep -q 'sp/Hello  World'  && ok "two spaces survive"        || fail "the two-space name was dropped"
	printf '%s' "$c" | grep -q 'sp/Hello World'   && ok "one space survives"        || fail "the one-space name was dropped"
	printf '%s' "$c" | grep -q 'sp/trailing '     && ok "a trailing space survives" || fail "the trailing space was lost"
fi

# ---- a directory in the path containing a space ----
d=$WORK/dir; make_pkgdir "$d"
{ printf 'install:\n'
  printf '\tinstall -d "$(DESTDIR)/usr/local/two words/deeper"\n'
  printf '\tprintf x > "$(DESTDIR)/usr/local/two words/deeper/file"\n'
} > "$d/Makefile"
run_ck "$d" --pkgname=ckt-spacedir --pkgversion=1.0 make install >"$WORK/p.log" 2>&1
deb=$(ls $PKGDIR/ckt-spacedir_1.0-1_*.deb 2>/dev/null | head -1)
if [ -z "$deb" ]; then
	fail "no package was built for the spaced directory"
	tail -15 "$WORK/p.log" | sed 's/^/      /'
else
	# awk on dpkg-deb -c cuts the path at its first space, so read the member
	# names straight out of the tar stream instead.
	dpkg-deb --fsys-tarfile "$deb" | tar tf - | grep -qx "./usr/local/two words/deeper/file" \
		&& ok "a parent directory with a space survives" \
		|| { fail "the file under the spaced directory is missing"
		     dpkg-deb --fsys-tarfile "$deb" | tar tf - | sed 's/^/        /'; }
fi

# ---- checkinstall run from a source directory with a space ----
d="$WORK/my source dir"; make_pkgdir "$d"
{ printf 'install:\n'
  printf '\tinstall -d $(DESTDIR)/usr/local/src-sp\n'
  printf '\tprintf x > $(DESTDIR)/usr/local/src-sp/file\n'
} > "$d/Makefile"
run_ck "$d" --pkgname=ckt-srcdir --pkgversion=1.0 make install >"$WORK/s.log" 2>&1
deb=$(ls $PKGDIR/ckt-srcdir_1.0-1_*.deb 2>/dev/null | head -1)
if [ -z "$deb" ]; then
	fail "no package was built from a source dir with a space"
	tail -15 "$WORK/s.log" | sed 's/^/      /'
else
	dpkg-deb --fsys-tarfile "$deb" | tar tf - | grep -qx "./usr/local/src-sp/file" \
		&& ok "a source directory with a space works" \
		|| fail "the file is missing when the source dir has a space"
fi

finish
