#!/bin/bash
# check_board_config.sh -- does a configure build an image for the right chip?
#
# The pin map (dirtyJtagConfig.h BOARD_TYPE, default BOARD_PICO2_W) and the
# SDK board (CMakeLists.txt PICO_BOARD) are chosen separately. Until
# 2026-10-24 their defaults disagreed: a plain `cmake ..` built the Pico 2 W's
# pin map as an RP2040 image, whose UF2 a Pico 2 W's bootloader ignores.
#
# Fresh configures (cmake --fresh) in build-check/, then make:
#   1. no options                            -> an RP2350 (ARM) UF2
#   2. PICO_BOARD=pico                       -> BOARD_PICO2_W on an RP2040: must not build
#   3. PICO_BOARD=pico2_w + BOARD_COLORLIGHT_DB -> the RP2040-STAMP's map on an RP2350: must not build
#   4. PICO_BOARD=solderparty_rp2040_stamp + BOARD_COLORLIGHT_DB -> an RP2040 UF2 (control)
#
#   PICO_SDK_PATH=~/workspaces/pico-sdk ./check_board_config.sh
set -u
: "${PICO_SDK_PATH:?export PICO_SDK_PATH=<your pico-sdk checkout>}"
cd "$(dirname "$0")" || exit 2
B=build-check
RP2040=e48bff56 RP2350_ARM_S=e48bff59
bad=0

# family IDs in a UF2 (blocks with the familyID flag 0x2000)
families() {
	python3 -c '
import struct, sys
d = open(sys.argv[1], "rb").read()
f = set()
for o in range(0, len(d) - 511, 512):
    m0, m1, flags = struct.unpack_from("<3I", d, o)
    if m0 == 0x0A324655 and m1 == 0x9E5D5157 and flags & 0x2000:
        f.add(struct.unpack_from("<I", d, o + 28)[0])
print(" ".join("%08x" % x for x in sorted(f)))' "$1"
}

build() {   # build <cmake options...>: fresh configure + make; output in $out
	rm -f "$B/dirtyJtag.elf" "$B/dirtyJtag.uf2"
	out=$(cmake --fresh -S . -B "$B" "$@" 2>&1) || return 1
	out=$(make -C "$B" -j"$(nproc)" 2>&1)
}

expect_uf2() {   # expect_uf2 <label> <family> <not-family> <cmake options...>
	local label=$1 want=$2 not=$3 got; shift 3
	if ! build "$@"; then
		echo "FAIL $label: did not build"; printf '%s\n' "$out" | command grep -m3 -E "error|Error"; bad=1
	elif [ ! -f "$B/dirtyJtag.uf2" ]; then
		echo "FAIL $label: built, but no UF2"; bad=1
	else
		got=$(families "$B/dirtyJtag.uf2")
		if [[ " $got " != *" $want "* || " $got " == *" $not "* ]]; then
			echo "FAIL $label: UF2 families [$got], want $want"; bad=1
		else
			echo "ok   $label: UF2 families [$got]"
		fi
	fi
}

expect_refused() {   # expect_refused <label> <message> <cmake options...>
	local label=$1 msg=$2; shift 2
	if build "$@"; then
		echo "FAIL $label: built (UF2 families [$(families "$B/dirtyJtag.uf2" 2>/dev/null)])"; bad=1
	elif ! command grep -q -- "$msg" <<<"$out"; then
		echo "FAIL $label: failed, but not with \"$msg\""; printf '%s\n' "$out" | command grep -m3 -E "error|Error"; bad=1
	else
		echo "ok   $label: refused: $(command grep -m1 -o -- "$msg.*" <<<"$out" | cut -c1-90)"
	fi
}

expect_uf2     "1 default            " "$RP2350_ARM_S" "$RP2040"
expect_refused "2 pico, PICO2_W map  " "BOARD_PICO2_W is" -DPICO_BOARD=pico
expect_refused "3 pico2_w, stamp map " "BOARD_COLORLIGHT_DB is" -DPICO_BOARD=pico2_w -DDJ_BOARD_TYPE=BOARD_COLORLIGHT_DB
expect_uf2     "4 stamp (control)    " "$RP2040" "$RP2350_ARM_S" -DPICO_BOARD=solderparty_rp2040_stamp -DDJ_BOARD_TYPE=BOARD_COLORLIGHT_DB
exit $bad
