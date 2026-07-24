# pico-dirtyJtag — Local FPGA Programming Setup

Custom build of [pico-dirtyJtag](https://github.com/phdussud/pico-dirtyJtag) for
programming FPGAs via [openFPGALoader](https://github.com/trabucayre/openFPGALoader).

Turns a Raspberry Pi Pico (or Pico 2W) into a USB JTAG adapter using the DirtyJTAG protocol.

## Supported FPGAs

Any FPGA supported by openFPGALoader, including:

- **Altera/Intel**: Cyclone III, Cyclone IV, Cyclone V, Cyclone 10 LP, MAX 10
- **Xilinx/AMD**: Artix-7 (xc7a15t–xc7a200t), Spartan-7, Kintex-7, Zynq-7000
- **Lattice**: ECP5, iCE40, MachXO2/3
- **Gowin**: GW1N, GW2A

Full list: `openFPGALoader --list-fpga`

## Pin Assignments

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

### Additional RP2040 GPIOs on FPGA connectors

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

The firmware exposes three USB interfaces:

| Device      | Interface     | Purpose                                 |
|-------------|---------------|-----------------------------------------|
| (vendor)    | JTAG vendor   | openFPGALoader JTAG (not a ttyACM)      |
| `/dev/ttyACM0` | CDC 0 (UART0) | UART bridge: GP0 (TX) / GP1 (RX)    |
| `/dev/ttyACM1` | CDC 1         | GPIO control: write `'1'`/`'0'` to toggle GP2 |

`ttyACM0` bridges UART0 to the FPGA pins U14/V14 (J12 header). In the dual-cluster
bitstream (`JopDualWukongTop`), the SDR cluster UART is routed to these pins, so the
SDR cluster output appears on `ttyACM0` without needing the CH340N adapter.

The baud rate is reconfigured by the host's serial port settings (via CDC line-coding).

## Local Modifications

Three changes from upstream for compatibility with Pico SDK 2.2.0 and our wiring:

1. **Multicore disabled** — `#define MULTICORE` commented out in `dirtyJtag.c`.
   The multicore USB handling causes bulk read timeouts with SDK 2.2.0.
2. **Custom JTAG pins** — in `dirtyJtagConfig.h` under `BOARD_PICO`:
   TDI=GP16, TDO=GP17, TCK=GP18, TMS=GP19 (JTAG on GP16-19).
3. **Dual CDC interfaces** — `CDC_UART_INTF_COUNT 2`: UART0 on GP0/GP1 bridged as
   ttyACM0; ttyACM1 used for GPIO control (GP2 UART-MUX select, no UART backing).

## Build

Requires Pico SDK 2.2.0 (`~/pico-sdk`).

For **Pico 2W** (RP2350):
```bash
cd ~/pico-dirtyJtag/build
rm -rf *
PICO_SDK_PATH=~/pico-sdk cmake -DPICO_BOARD=pico2_w ..
make -j$(nproc)
```

For **original Pico** (RP2040):
```bash
cd ~/pico-dirtyJtag/build
rm -rf *
PICO_SDK_PATH=~/pico-sdk cmake ..
make -j$(nproc)
```

Output: `build/dirtyJtag.elf` and `build/dirtyJtag.uf2`

## Flash to Pico

Via SWD debug probe (requires Raspberry Pi's OpenOCD fork for RP2350 —
installed at `/usr/local/bin/openocd` from `~/raspberrypi-openocd`):

```bash
# Pico 2W (RP2350)
sudo /usr/local/bin/openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg \
  -c "adapter speed 5000; program ~/pico-dirtyJtag/build/dirtyJtag.elf verify reset exit"

# Original Pico (RP2040) — system openocd works
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg \
  -c "adapter speed 5000; program ~/pico-dirtyJtag/build/dirtyJtag.elf verify reset exit"
```

Or via UF2: hold BOOTSEL, connect USB, copy `dirtyJtag.uf2` to the mounted drive.

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
# Switch to dirtyJtag (FPGA programming) — Pico 2W
sudo /usr/local/bin/openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg \
  -c "adapter speed 5000; program ~/pico-dirtyJtag/build/dirtyJtag.elf verify reset exit"

# Switch to pico-fpga (LA / UART / JTAG tool) — Pico 2W
sudo /usr/local/bin/openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg \
  -c "adapter speed 5000; program ~/pico-fpga/build/pico_fpga.elf verify reset exit"
```

## VM Note

When running inside a VM (QEMU/KVM), the DirtyJTAG USB device (`1209:c0ca`) must
be passed through to the guest after flashing. The pico-fpga device (`2e8a:000a`)
uses a different VID:PID, so both need separate passthrough rules.
