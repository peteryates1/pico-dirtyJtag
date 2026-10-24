#!/bin/bash
# check_uf2_rule.sh [BUILD_DIR] -- does `make` remake dirtyJtag.uf2?
#
# pico-sdk makes the UF2 as a POST_BUILD step of linking the ELF, and a
# Makefile build does not track POST_BUILD outputs: a UF2 missing beside an
# up-to-date ELF was never made again, and `make` said nothing (2026-10-23).
#
# In BUILD_DIR (default build-pico2w), after a full `make`: remove the UF2,
# run `make`; then make the UF2 older than the ELF, run `make`. Each time the
# UF2 must come back from the ELF WITHOUT a relink -- a relink would remake it
# through the POST_BUILD step and pass for the wrong reason.
#
#   ./check_uf2_rule.sh build-pico2w
set -u
B=${1:-build-pico2w}
ELF=$B/dirtyJtag.elf
UF2=$B/dirtyJtag.uf2
BAK=$B/dirtyJtag.uf2.check-bak

fail() { echo "FAIL ($B): $*"; [ -f "$BAK" ] && [ ! -f "$UF2" ] && mv "$BAK" "$UF2"; exit 1; }
run_make() { out=$(make -C "$B" 2>&1) || { printf '%s\n' "$out" | tail -20; fail "make failed"; }; }
is_uf2() { [ "$(head -c 4 "$1" | od -An -tx4 | tr -d ' ')" = 0a324655 ]; }

[ -f "$B/Makefile" ] || { echo "usage: $0 BUILD_DIR (a configured build directory)"; exit 2; }
run_make
[ -f "$ELF" ] || fail "no $ELF after make"
[ -f "$UF2" ] || fail "no $UF2 after a full make"
elf_t=$(stat -c %y "$ELF")

# 1. A missing UF2.
mv "$UF2" "$BAK"
run_make
[ -f "$UF2" ] || fail "make did not remake a missing UF2"
[ "$(stat -c %y "$ELF")" = "$elf_t" ] || fail "make relinked the ELF to remake the UF2"
is_uf2 "$UF2" || fail "$UF2 is not a UF2 file"
rm -f "$BAK"

# 2. A UF2 older than the ELF.
touch -d "@$(( $(stat -c %Y "$ELF") - 60 ))" "$UF2"
run_make
[ "$UF2" -nt "$ELF" ] || fail "make did not remake a UF2 older than the ELF"
[ "$(stat -c %y "$ELF")" = "$elf_t" ] || fail "make relinked the ELF to remake the UF2"

echo "PASS ($B): make remakes a missing or stale UF2 from the ELF, no relink"
