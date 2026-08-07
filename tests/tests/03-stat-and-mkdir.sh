#!/bin/bash
# mkdir -p has to work (#717778), and what it creates has to be visible to
# the stat calls a build makes afterwards (#1116590).
. "$HARNESS"

d=$WORK/pkg; make_pkgdir "$d"
cat > "$d/mk.sh" <<'INNER'
#!/bin/sh
mkdir -p /usr/local/ckt/a/b/c && touch /usr/local/ckt/a/b/c/file && echo "OK mkdir -p then create a file in it"
mkdir -p /nonexistentroot/x/y && echo "OK mkdir -p where no component exists"
( cd /usr/local && mkdir -p rel/a/b ) && echo "OK relative mkdir -p"
install -d /usr/local/ckt/inst/x && echo "OK install -d"
D=/usr/local/ckt/plugin
mkdir -p $D
test -d $D && echo "OK test -d sees a directory made during the install"
test -e $D/../plugin && echo "OK test -e likewise"
stat -c %s /usr/local/ckt/a/b/c/file >/dev/null 2>&1 && echo "OK stat(1) sees a file made during the install"
ls /usr/local/ckt/a/b/c 2>/dev/null | grep -q file && echo "OK ls sees it too"
INNER
chmod +x "$d/mk.sh"

out=$(run_ck "$d" --pkgname=ckt-mkdir --pkgversion=1.0 ./mk.sh 2>&1)
for want in "mkdir -p then create a file in it" "mkdir -p where no component exists" \
            "relative mkdir -p" "install -d" \
            "test -d sees a directory made during the install" "test -e likewise" \
            "stat(1) sees a file made during the install" "ls sees it too"; do
	if printf '%s' "$out" | grep -qF "OK $want"; then ok "$want"; else fail "$want"; fi
done
finish
