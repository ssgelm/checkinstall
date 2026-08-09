#!/bin/bash
# cmake's file(INSTALL) stats the destination before copying (#1051180).
. "$HARNESS"

command -v cmake >/dev/null || { skip "cmake is not installed"; finish; }

d=$WORK/pkg; make_pkgdir "$d"
cat > "$d/CMakeLists.txt" <<'INNER'
cmake_minimum_required(VERSION 3.10)
project(ckt C)
add_executable(ckt ckt.c)
install(TARGETS ckt DESTINATION bin)
install(FILES ckt.c DESTINATION share/ckt)
INNER
printf 'int main(void){return 0;}\n' > "$d/ckt.c"
mkdir -p "$d/build"
( cd "$d/build" && cmake -DCMAKE_INSTALL_PREFIX=/usr/local .. && make ) >"$WORK/cmake.log" 2>&1 || {
	fail "the cmake project itself would not build"; tail -5 "$WORK/cmake.log" | sed 's/^/      /'; finish; }
make_pkgdir "$d/build"

out=$(run_ck "$d/build" --pkgname=ckt-cmake --pkgversion=1.0 make install 2>&1)
if printf '%s' "$out" | grep -q "Installation successful"; then
	ok "cmake install under fstrans"
else
	fail "cmake install under fstrans"
	evidence 15 "$out"
fi
finish
