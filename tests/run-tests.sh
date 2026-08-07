#!/bin/bash
# Build checkinstall from a source tree and run the tests against it.
#
#   ./run-tests.sh                     all tests
#   ./run-tests.sh tests/05-linkat.sh  one test
#   CHECKINSTALL_SRC=/path ./run-tests.sh
#   CHECKINSTALL_DEB=/path/to.deb ./run-tests.sh   test a built package
#
# Apply the quilt patches in the source tree first to test the fixed
# checkinstall; leave them off to watch the tests fail against pristine
# upstream.

set -u

here=$(cd "$(dirname "$0")" && pwd)
CK_SRC=${CHECKINSTALL_SRC:-$(cd "$here/.." && pwd)}

case $here in
	/tmp/*|/var/tmp/*)
		echo "error: installwatch excludes /tmp and /var/tmp, so the tests"
		echo "       cannot run from $here. Use a checkout elsewhere."
		exit 2;;
esac

CK_DEB=${CHECKINSTALL_DEB:-}

if [ -n "$CK_DEB" ]; then
	[ -f "$CK_DEB" ] || { echo "error: no such package: $CK_DEB"; exit 2; }
	echo "checkinstall package: $CK_DEB"
	# 01 needs the source of test-installwatch.c and skips without it
	[ -f "${CK_SRC:-}/installwatch/test-installwatch.c" ] || CK_SRC=""
elif [ -z "${CK_SRC:-}" ] || [ ! -f "$CK_SRC/checkinstall.in" ]; then
	echo "error: no checkinstall source tree or package found."
	echo "       Set CHECKINSTALL_SRC=/path/to/checkinstall or"
	echo "       CHECKINSTALL_DEB=/path/to/package.deb."
	exit 2
else
	echo "checkinstall source: $CK_SRC"
fi

CKPREFIX=$(mktemp -d "$here/.prefix.XXXXXX")
TESTTMP=$(mktemp -d "$here/.tmp.XXXXXX")
PKGDIR=$TESTTMP/packages
mkdir -p "$CKPREFIX"/{sbin,bin,lib/checkinstall} "$PKGDIR"
trap 'rm -rf "$CKPREFIX" "$TESTTMP"' EXIT

# The rc file is read after the environment, so anything it sets wins. Point
# it at the prefix under test rather than at whatever checkinstall may be
# installed on this machine, and settle the package format the tests expect.
write_rc() {
	sed -e "s|^PAK_DIR=.*|PAK_DIR=$PKGDIR|" \
	    -e "s|^INSTALLWATCH_PREFIX=.*|INSTALLWATCH_PREFIX=\"$CKPREFIX\"|" \
	    -e "s|^INSTYPE=.*|INSTYPE=\"D\"|" \
	    "$1" > "$TESTTMP/checkinstallrc"
}

if [ -n "$CK_DEB" ]; then
	echo -n "unpacking... "
	dpkg-deb -x "$CK_DEB" "$CKPREFIX/root" || exit 2
	# the package puts everything under /usr, and both scripts take their
	# prefix from the environment, so it runs from where it was unpacked
	CKPREFIX=$CKPREFIX/root/usr
	write_rc "$CKPREFIX/../etc/checkinstallrc"
	echo "ok"
else
	echo -n "building... "
	# Let the tree install itself, so the layout is the one it expects.
	# Trees whose install target still wants root for chown cannot do
	# that, so place the files ourselves when it fails.
	if ! make -C "$CK_SRC" install PREFIX="$CKPREFIX" > "$TESTTMP/build.log" 2>&1; then
		if ! make -C "$CK_SRC" PREFIX="$CKPREFIX" >> "$TESTTMP/build.log" 2>&1; then
			echo "failed:"; tail -20 "$TESTTMP/build.log"; exit 2
		fi
		mkdir -p "$CKPREFIX/sbin" "$CKPREFIX/bin" "$CKPREFIX/lib"
		install -m 0755 "$CK_SRC/checkinstall" "$CKPREFIX/sbin/"
		install -m 0755 "$CK_SRC/installwatch/installwatch.so" "$CKPREFIX/lib/"
		sed "s|#PREFIX#|$CKPREFIX|" "$CK_SRC/installwatch/installwatch" \
			> "$CKPREFIX/bin/installwatch"
		chmod +x "$CKPREFIX/bin/installwatch"
	fi
	write_rc "$CK_SRC/checkinstallrc-dist"
	echo "ok"
fi

CKBIN=$(find "$CKPREFIX" -name checkinstall -type f -perm -u+x | head -1)
IW_SO=$(find "$CKPREFIX" -name installwatch.so | head -1)
if [ -z "$CKBIN" ] || [ -z "$IW_SO" ]; then
	echo "error: no checkinstall or installwatch.so under $CKPREFIX"
	exit 2
fi

export CK="$CKBIN"
export IW_SO
export CKPREFIX CKRC="$TESTTMP/checkinstallrc" CK_SRC PKGDIR TESTTMP
export CK_MODE=$([ -n "$CK_DEB" ] && echo deb || echo src)
export HARNESS="$here/harness.sh"

run=0; failed=0
for t in "${@:-$here/tests/}"*; do
	case $t in *.sh) ;; *) continue;; esac
	echo
	echo "== $(basename "$t")"
	run=$((run + 1))
	bash "$t" || failed=$((failed + 1))
done

echo
echo "$((run - failed))/$run test files passed"
[ "$failed" -eq 0 ]
