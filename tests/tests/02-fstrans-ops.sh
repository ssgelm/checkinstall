#!/bin/bash
# The everyday install operations, run under --fstrans=yes. Each one has to
# see the files the ones before it created.
. "$HARNESS"

need "$CK_PKGTOOL" "no tool for $CK_PKGTYPE packages"

d=$WORK/pkg; make_pkgdir "$d"
cat > "$d/ops.sh" <<'INNER'
#!/bin/sh
t() { desc="$1"; shift; if "$@" >/dev/null 2>&1; then echo "OK $desc"; else echo "NO $desc"; fi; }
B=/usr/local/ckt
mkdir -p $B/a/b $B/extract
echo hello > $B/a/b/f1
t "cp file"       cp $B/a/b/f1 $B/a/b/f2
t "cp -r dir"     cp -r $B/a $B/acopy
t "mv file"       mv $B/a/b/f2 $B/a/b/f3
t "rm file"       rm $B/a/b/f3
t "rm -r dir"     rm -r $B/acopy
t "ln -s"         ln -s f1 $B/a/b/link
t "ln hard"       ln $B/a/b/f1 $B/a/b/hard
t "chmod"         chmod 600 $B/a/b/f1
t "touch -r"      touch -r $B/a/b/f1 $B/a/b/f4
t "sed -i"        sed -i s/hello/world/ $B/a/b/f1
t "install file"  install -m 644 $B/a/b/f1 $B/a/b/f5
t "install -D"    install -D -m 644 $B/a/b/f1 $B/d1/d2/f6
t "tar -c"        tar -cf $B/a.tar -C $B a
t "tar -x"        tar -xf $B/a.tar -C $B/extract
t "find -delete"  find $B/extract -name f4 -delete
t "ar + ranlib"   sh -c "cd $B && ar rcs lib.a a/b/f1 && ranlib lib.a"
t "read back"     grep -q world $B/a/b/f1
t "ls dir"        sh -c "ls $B/a/b | grep -q f5"
t "stat(1)"       stat -c %s $B/a/b/f1
INNER
chmod +x "$d/ops.sh"

out=$(run_ck "$d" --pkgname=ckt-ops --pkgversion=1.0 ./ops.sh 2>&1)
while read -r verdict desc; do
	case $verdict in
		OK) ok "$desc";;
		NO) fail "$desc";;
	esac
done < <(printf '%s\n' "$out" | grep -E "^(OK|NO) ")
finish
