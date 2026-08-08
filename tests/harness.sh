# Sourced by every test. run-tests.sh exports CK, IW_SO, CKPREFIX, CKRC,
# CK_SRC and PKGDIR before running us.

set -u

: "${CK:?run the tests through ./run-tests.sh}"

FAILURES=0
ok()   { printf '  ok    %s\n' "$*"; }
fail() { printf '  FAIL  %s\n' "$*"; FAILURES=$((FAILURES + 1)); }
skip() { printf '  skip  %s\n' "$*"; }
finish() { [ "$FAILURES" -eq 0 ]; exit $?; }

# Per-test scratch directory. It has to live outside /tmp and /var/tmp:
# installwatch excludes those, so a test run from there would watch nothing.
WORK=$(mktemp -d "$TESTTMP/work.XXXXXX")
trap 'rm -rf "$WORK"' EXIT

# run_ck <dir> <checkinstall args...>
run_ck() {
	local d=$1; shift
	( cd "$d" && CHECKINSTALLRC="$CKRC" PREFIX="$CKPREFIX" \
	  INSTALLWATCH_PREFIX="$CKPREFIX" "$CK" --install=no --default "$@" )
}

# run_iw <root> <translate 0|1> <command...>
run_iw() {
	local root=$1 transl=$2; shift 2
	rm -rf "$root"; mkdir -p "$root"
	INSTW_ROOTPATH="$root" INSTW_BACKUP=1 INSTW_TRANSL="$transl" \
	INSTW_LOGFILE="$root/logfile" INSTW_DBGFILE="$root/dbgfile" \
	INSTW_DBGLVL=0 INSTW_EXCLUDE="${INSTW_EXCLUDE:-/dev,/proc,/tmp,/var/tmp,}" \
	LD_PRELOAD="$IW_SO" "$@"
}

# a minimal source directory checkinstall will accept
make_pkgdir() {
	local d=$1
	mkdir -p "$d"
	printf 'test package for the checkinstall test suite\n' > "$d/description-pak"
}
