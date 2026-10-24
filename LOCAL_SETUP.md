# pico-dirtyJtag — Local FPGA Programming Setup

Custom build of [pico-dirtyJtag](https://github.com/phdussud/pico-dirtyJtag) for
programming FPGAs via [openFPGALoader](https://github.com/trabucayre/openFPGALoader).

Turns a Raspberry Pi Pico 2 W (the default build), a Pico or an RP2040-STAMP into
a USB JTAG adapter using the DirtyJTAG protocol, with two USB-UART bridges.

## Supported FPGAs

Any FPGA supported by openFPGALoader, including:

- **Altera/Intel**: Cyclone III, Cyclone IV, Cyclone V, Cyclone 10 LP, MAX 10
- **Xilinx/AMD**: Artix-7 (xc7a15t–xc7a200t), Spartan-7, Kintex-7, Zynq-7000
- **Lattice**: ECP5, iCE40, MachXO2/3
- **Gowin**: GW1N, GW2A

Full list: `openFPGALoader --list-fpga`

## Boards

The pin map (`DJ_BOARD_TYPE`, a block in `dirtyJtagConfig.h`) and the SDK board
(`PICO_BOARD`) are chosen separately; both default to the Pico 2 W. The two pin
maps we added refuse to compile for the wrong chip, with an `#error` naming the
fix.

| Board        | `DJ_BOARD_TYPE`           | `PICO_BOARD`               | Build directory    | Wired to                            |
|--------------|---------------------------|----------------------------|--------------------|-------------------------------------|
| Pico 2 W     | `BOARD_PICO2_W` (default) | `pico2_w` (default)        | `build-pico2w`     | QMTECH Wukong (XC7A100T) JTAG header |
| RP2040-STAMP | `BOARD_COLORLIGHT_DB`     | `solderparty_rp2040_stamp` | `build-colorlight` | Colorlight i5 base board            |
| Pico         | `BOARD_PICO`              | `pico`                     | `build`            | QMTECH RP2040 daughterboard         |

## Pin Assignments

### Pico 2 W on the Wukong (`BOARD_PICO2_W`)

| Signal        | GPIO        | Notes                     |
|---------------|-------------|---------------------------|
| TCK           | GP0         |                           |
| TDO           | GP1         |                           |
| TDI           | GP2         |                           |
| TMS           | GP3         |                           |
| UART0 TX / RX | GP12 / GP13 | first ttyACM, Wukong J11  |
| UART1 TX / RX | GP4 / GP5   | second ttyACM, Wukong J11 |

RST/TRST are on GP20/21, unconnected. The Pico 2 W's LED is on the CYW43
(WL_GPIO0), so the LED pins are disabled. The JOP repo's
`fpga/scripts/usb_serial_map` (`wukong-pico-0`, `wukong-pico-1`) records the J11
pins.

### RP2040-STAMP on the Colorlight base board (`BOARD_COLORLIGHT_DB`)

JTAG is routed on the board: GPIO0-3 = TCK, TMS, TDI, TDO. The first ttyACM is
uart1 on GPIO4 (TX) / GPIO5 (RX), the second is uart0 on GPIO16 (TX) / GPIO17
(RX). RST/TRST are not defined: every other GPIO goes straight to an FPGA ball.

### Pico on the QMTECH RP2040 daughterboard (`BOARD_PICO`)

| Signal   | GPIO      | FPGA Connector | Usage    |
|----------|-----------|----------------|----------|
| UART0 TX | GP0       | J3_IO7         | UART0 TX |
| UART0 RX | GP1       | J3_IO8         | UART0 RX |
| UART1 TX | GP4       | J2_IO42        | UART1 TX |
| UART1 RX | GP5       | J2_IO41        | UART1 RX |
| TDI      | GP16      | (JTAG direct)  | TDI      |
| TDO      | GP17      | (JTAG direct)  | TDO      |
| TCK      | GP18      | (JTAG direct)  | TCK      |
| TMS      | GP19      | (JTAG direct)  | TMS      |
| LED      | GP25      | RP2040_LED     |          |

JTAG pins (GP16-19) connect directly to the FPGA JTAG chain, not through the
I/O connectors. UART pins are routed through FPGA connector pins and require
matching FPGA I/O to be configured on those pins.

#### Additional RP2040 GPIOs on FPGA connectors

| GPIO      | FPGA Connector |
|-----------|----------------|
| GP2       | J2_IO44        |
| GP3       | J2_IO43        |
| GP6       | J2_IO40        |
| GP7       | J2_IO39        |
| GP8       | J2_IO38        |
| GP9       | J2_IO37        |
| GP10      | J2_IO36        |
| GP11      | J2_IO35        |
| GP12      | J2_IO34        |
| GP13      | J2_IO33        |
| GP14      | J2_IO32        |
| GP15      | J2_IO31        |
| GP20      | J2_IO30        |
| GP21      | J2_IO29        |
| GP22      | J2_IO28        |
| GP23      | J2_IO27        |
| GP24      | RP2040_KEY     |
| GP26_ADC0 | J2_IO26        |
| GP27_ADC1 | J2_IO25        |
| GP28_ADC2 | J2_IO16        |
| GP29_ADC3 | J2_IO15        |

RST (GP20) and TRST (GP21) are configured but optional — most boards only need
the four JTAG signals plus GND.

## USB Interfaces

The adapter enumerates as `1209:c0ca` (product "Pico Dirty JTAG"); its serial
number is the chip's unique ID.

| Interface      | Purpose                                                |
|----------------|--------------------------------------------------------|
| vendor (bulk)  | JTAG for openFPGALoader (`-c dirtyJtag`), not a ttyACM |
| CDC 0 (`if01`) | UART bridge on the board's first UART (see above)      |
| CDC 1 (`if03`) | UART bridge on the second UART                         |

ttyACM numbers change between plugs: open
`/dev/serial/by-id/usb-Jean_THOMAS_Pico_Dirty_JTAG_<serial>-if01` (or `-if03`).
The baud rate follows the host's serial port settings (CDC line coding).

On the Wukong, single-system JOP builds use the board's own CH340N console.
Builds generated with `uart=PICO_UART0` use CDC 0, and the dual-cluster build
(`JopDualWukongTop`) uses both: the SDR cluster on CDC 0, the DDR3 cluster on
CDC 1.

## Local Modifications

Changes from upstream for compatibility with Pico SDK 2.2.0 and our wiring:

1. **Single core** — upstream's `MULTICORE` USB handling caused bulk read
   timeouts with SDK 2.2.0. It was switched off in `ade97b0` and removed in the
   DJTAG3 rewrite (`2c793fd`, up to 4000 bits per transfer, ~394 KB/s vs
   ~300 KB/s).
2. **Board pin maps** in `dirtyJtagConfig.h` (see Boards): `BOARD_PICO2_W` (the
   default) and `BOARD_COLORLIGHT_DB` added, `BOARD_PICO` changed to the QMTECH
   daughterboard's GP16-19. The two we added check their chip
   (`check_board_config.sh`).
3. **Two CDC UARTs** — `CDC_UART_INTF_COUNT 2`. The second only works since
   `b1241f3`: until then just the first UART was initialised, a dead UART-MUX
   select feature swallowed CDC 1's bytes, and a closed port starved the others.
4. **CDC↔UART bridge reliability fixes** in `cdc_uart.c` — the UART bridge is used
   for JOP serial download/console, which streams with no per-byte handshake, so a
   single lost byte hangs the transfer. Two upstream bugs dropped bytes:
   - `dma_handler` processed uninitialised `uart_devices[]` entries, corrupting the
     TX DMA write address so nothing transmitted — added a NULL guard (`488f86f`).
   - RX→USB forwarding batched with a `FULL_SWO_PACKET`/`n_checks>4` heuristic (from
     the SWO-trace use case) and left the tail of small bursts unforwarded (the
     4-byte download checksum came back short; console output lost characters).
     Now forwards immediately and clamps each `tud_cdc_n_write` to the contiguous
     span before the ring wrap (`36809d0`). Result: lossless JOP download at full
     2 Mbaud, no host throttling. Lowering the FPGA baud is not a fix — it just
     moves the loss to the host→FPGA direction.
5. **The UF2 is a make target** (`CMakeLists.txt`). The SDK makes it only as a
   post-link step, so a UF2 missing beside an up-to-date ELF was never made again
   (2026-10-23). `make` now remakes a missing or stale one (`check_uf2_rule.sh`).

## Build

Pico SDK 2.2.0 at `~/workspaces/pico-sdk`, this repo at
`~/workspaces/pico-dirtyJtag`. Use one build directory per board: each caches its
board.

```bash
cd ~/workspaces/pico-dirtyJtag

# Pico 2 W (the default)
PICO_SDK_PATH=~/workspaces/pico-sdk cmake -S . -B build-pico2w -DPICO_BOARD=pico2_w
make -C build-pico2w -j$(nproc)

# RP2040-STAMP on the Colorlight base board
PICO_SDK_PATH=~/workspaces/pico-sdk cmake -S . -B build-colorlight \
  -DPICO_BOARD=solderparty_rp2040_stamp -DDJ_BOARD_TYPE=BOARD_COLORLIGHT_DB
make -C build-colorlight -j$(nproc)

# Pico on the QMTECH RP2040 daughterboard
PICO_SDK_PATH=~/workspaces/pico-sdk cmake -S . -B build -DPICO_BOARD=pico -DDJ_BOARD_TYPE=BOARD_PICO
make -C build -j$(nproc)
```

`cmake` is needed once per directory. After that, `make -C <dir>` alone rebuilds,
re-running CMake itself when `CMakeLists.txt` changes. Output:
`<dir>/dirtyJtag.elf` (for SWD) and `<dir>/dirtyJtag.uf2` (for BOOTSEL).

Two scripts test the build:

```bash
./check_uf2_rule.sh build-pico2w        # make remakes a missing or stale UF2
PICO_SDK_PATH=~/workspaces/pico-sdk ./check_board_config.sh
                                        # board/pin-map pairs (in build-check/)
```

## Flash to the Pico

**BOOTSEL** (no probe needed): hold BOOTSEL while plugging in USB, then

```bash
picotool load -v -x build-pico2w/dirtyJtag.uf2    # -x: run it afterwards
```

or copy the `.uf2` to the drive that appears. A Pico 2 W cannot run an RP2040
UF2, which is why each pin map checks its chip.

**SWD**, through a CMSIS-DAP probe. Name the probe with `adapter serial`: with more
than one CMSIS-DAP device on the bus, OpenOCD takes the first it finds.

```bash
# Pico 2 W (RP2350): Raspberry Pi's OpenOCD fork, /usr/local/bin/openocd
# (built from ~/workspaces/raspberrypi-openocd)
/usr/local/bin/openocd -f interface/cmsis-dap.cfg -c "adapter serial <probe>" \
  -f target/rp2350.cfg -c "adapter speed 5000" \
  -c "program build-pico2w/dirtyJtag.elf verify reset exit"

# RP2040 (stamp or Pico)
/usr/local/bin/openocd -f interface/cmsis-dap.cfg -c "adapter serial <probe>" \
  -f target/rp2040.cfg -c "adapter speed 5000" \
  -c "program build-colorlight/dirtyJtag.elf verify reset exit"
```

Probes (2026-10-24): the pico-usb-debug-jtag carrier's J2 (`e66440a2`) is on the
Wukong's Pico 2 W; the Raspberry Pi Debug Probe (`E6647C740343412F`) is on the
carrier's J3. The JOP repo's `fpga/scripts/jtag_probe_map` has both serials
(`ep4cgx150` is the carrier, `debugprobe` the Debug Probe).

## Install openFPGALoader

```bash
sudo apt install openfpgaloader
```

The JOP flow runs `/usr/local/bin/openFPGALoader`, a local build of v1.1.1 whose
`--busdev-num` also works for dirtyJtag. With more than one dirtyJtag plugged in,
pick one with it: `--busdev-num $(~/workspaces/jop-spinalhdl/fpga/scripts/jtag_probe_map --busdev wukong)`.

## Usage

### Detect

```bash
sudo openFPGALoader -c dirtyJtag --detect
```

Example output (Cyclone IV EP4CE115):

```
index 0:
    idcode 0x20f70dd
    manufacturer altera
    family cyclone III/IV/10 LP
    model  EP3C120/EP4CE115/10CL120
    irlength 10
```

### Program SRAM (volatile — lost on power cycle)

```bash
# Altera/Intel (.rbf)
sudo openFPGALoader -c dirtyJtag -m design.rbf

# Xilinx (.bit)
sudo openFPGALoader -c dirtyJtag design.bit
```

### Program flash (non-volatile)

```bash
sudo openFPGALoader -c dirtyJtag --write-flash design.rbf
sudo openFPGALoader -c dirtyJtag --write-flash design.bit
```

### Verbose / debug output

```bash
sudo openFPGALoader -c dirtyJtag -v -m design.rbf           # verbose
sudo openFPGALoader -c dirtyJtag --verbose-level 2 --detect  # max debug
```

## Switching Between Firmwares

The same Pico and wiring supports both pico-dirtyJtag (for openFPGALoader FPGA
programming) and pico-fpga (for logic analyzer, UART bridge, JTAG scripting).
Flash either one as above: `build-pico2w/dirtyJtag.elf` here, or
`~/workspaces/pico-fpga/build/pico_fpga.elf` once pico-fpga is built.

## VM Note

When running inside a VM (QEMU/KVM), the DirtyJTAG USB device (`1209:c0ca`) must
be passed through to the guest after flashing. The pico-fpga device (`2e8a:000a`)
and the BOOTSEL bootloaders (`2e8a:000f` RP2350, `2e8a:0003` RP2040) use other
VID:PIDs, so each needs its own passthrough rule.
