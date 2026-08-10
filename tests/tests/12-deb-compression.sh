#!/bin/bash
# --debcompression picks what dpkg-deb packs the data member with.
. "$HARNESS"

need_pkgtype D
need dpkg-deb "this one reads a .deb"

build() { # <name> <checkinstall args...>
	local n=$1; shift
	local d=$WORK/$n; make_pkgdir "$d"
	printf 'payload\n' > "$d/ckt.txt"
	# A literal tab starts each recipe line, so no indented heredoc here
	{ printf 'install:\n'
	  printf '\tinstall -d $(DESTDIR)/usr/share/ckt-%s\n' "$n"
	  printf '\tinstall -m 644 ckt.txt $(DESTDIR)/usr/share/ckt-%s/\n' "$n"
	} > "$d/Makefile"
	run_ck "$d" --pkgname="ckt-$n" --pkgversion=1.0 "$@" make install \
		>"$WORK/$n.log" 2>&1
	ls $PKGDIR/ckt-${n}_1.0-1_*.deb 2>/dev/null | head -1
}

member() { ar t "$1" 2>/dev/null | grep '^data\.tar'; }

deb=$(build default)
if [ -n "$deb" ]; then
	ok "a package still builds with no compression named ($(member "$deb"))"
else
	fail "no package was built without the option"
	finish
fi

# older dpkg-deb builds only some of these. Ask it rather than assume.
supported() {
	d=$WORK/probe; rm -rf "$d"; mkdir -p "$d/DEBIAN"
	printf 'Package: p\nVersion: 1\nArchitecture: all\nMaintainer: t\nDescription: t\n' \
		> "$d/DEBIAN/control"
	dpkg-deb -Z"$1" --build "$d" "$WORK/probe.deb" >/dev/null 2>&1
}

for t in gzip:data.tar.gz none:data.tar zstd:data.tar.zst xz:data.tar.xz; do
	want=${t#*:}; type=${t%%:*}
	if ! supported "$type"; then
		skip "--debcompression=$type: this dpkg-deb cannot build $type"
		continue
	fi
	deb=$(build "$type" "--debcompression=$type")
	if [ -z "$deb" ]; then
		fail "--debcompression=$type built no package"
		sed -n '/Failed to build/,$p' "$WORK/$type.log" | head -5 | sed 's/^/      /'
		continue
	fi
	got=$(member "$deb")
	if [ "$got" = "$want" ]; then
		ok "--debcompression=$type gave $got"
	else
		fail "--debcompression=$type gave $got, wanted $want"
	fi
done

# A bad value has to stop us before the install command runs, not after
d=$WORK/bad; make_pkgdir "$d"
cat > "$d/Makefile" <<'INNER'
install:
	touch $(WORK)/install-ran
INNER
out=$(run_ck "$d" --pkgname=ckt-bad --pkgversion=1.0 --debcompression=bogus \
	make install 2>&1)
if printf '%s' "$out" | grep -q "invalid value"; then
	ok "an unknown compression is refused"
else
	fail "an unknown compression was not refused"
	printf '%s\n' "$out" | head -5 | sed 's/^/      /'
fi
if [ -e "$WORK/install-ran" ]; then
	fail "the install command ran before the value was checked"
else
	ok "nothing was installed before the value was checked"
fi

finish
