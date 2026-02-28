# pico-dirtyJtag — Local FPGA Programming Setup

Custom build of [pico-dirtyJtag](https://github.com/phdussud/pico-dirtyJtag) for
programming FPGAs via [openFPGALoader](https://github.com/trabucayre/openFPGALoader).

Turns a Raspberry Pi Pico into a USB JTAG adapter using the DirtyJTAG protocol.

## Supported FPGAs

Any FPGA supported by openFPGALoader, including:

- **Altera/Intel**: Cyclone III, Cyclone IV, Cyclone V, Cyclone 10 LP, MAX 10
- **Xilinx/AMD**: Artix-7 (xc7a15t–xc7a200t), Spartan-7, Kintex-7, Zynq-7000
- **Lattice**: ECP5, iCE40, MachXO2/3
- **Gowin**: GW1N, GW2A

Full list: `openFPGALoader --list-fpga`

## Pin Assignments

| Signal | GPIO | Pico Pin |
|--------|------|----------|
| TCK    | GP2  | 4        |
| TMS    | GP3  | 5        |
| TDI    | GP4  | 6        |
| TDO    | GP5  | 7        |
| GND    | —    | 3, 8     |

These match the pico-fpga JTAG pin assignments so the same wiring works for both
firmwares.

RST (GP20) and TRST (GP21) are configured but optional — most boards only need
the four JTAG signals plus GND.

## Local Modifications

Two changes from upstream for compatibility with Pico SDK 2.2.0 and our wiring:

1. **Multicore disabled** — `#define MULTICORE` commented out in `dirtyJtag.c`.
   The multicore USB handling causes bulk read timeouts with SDK 2.2.0.
2. **Custom pins + no CDC UART** — in `dirtyJtagConfig.h` under `BOARD_PICO`:
   TCK=GP2, TMS=GP3, TDI=GP4, TDO=GP5, `CDC_UART_INTF_COUNT 0`.

## Build

Requires Pico SDK 2.2.0 (`~/pico-sdk`).

```bash
cd ~/pico-dirtyJtag/build
PICO_SDK_PATH=~/pico-sdk cmake ..
make -j$(nproc)
```

Output: `build/dirtyJtag.elf` and `build/dirtyJtag.uf2`

## Flash to Pico

Via SWD debug probe:

```bash
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg \
  -c "adapter speed 5000; program ~/pico-dirtyJtag/build/dirtyJtag.elf verify reset exit"
```

Or via UF2: hold BOOTSEL, connect USB, copy `dirtyJtag.uf2` to the RPI-RP2 drive.

The Pico enumerates as USB device `1209:c0ca` ("DirtyJTAG").

## Install openFPGALoader

```bash
sudo apt install openfpgaloader
```

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

```bash
# Switch to dirtyJtag (FPGA programming)
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg \
  -c "adapter speed 5000; program ~/pico-dirtyJtag/build/dirtyJtag.elf verify reset exit"

# Switch to pico-fpga (LA / UART / JTAG tool)
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg \
  -c "adapter speed 5000; program ~/pico-fpga/build/pico_fpga.elf verify reset exit"
```

## VM Note

When running inside a VM (QEMU/KVM), the DirtyJTAG USB device (`1209:c0ca`) must
be passed through to the guest after flashing. The pico-fpga device (`2e8a:000a`)
uses a different VID:PID, so both need separate passthrough rules.
