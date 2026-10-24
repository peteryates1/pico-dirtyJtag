# PICO-DIRTYJTAG

This code allows the Pico to become a JTAG cable.  It uses the PIO unit to produce and capture the JTAG signals.  This is a port of the excellent [DirtyJtag project](https://github.com/jeanthom/DirtyJTAG)

An additional feature provides a bridge to an external UART.  This is often useful when the target provides debugging output or a console over a UART.

## Pinout

This fork builds three boards, each with its own pin map: [Building](#building-pico-dirtyjtag)
lists them, and [LOCAL_SETUP.md](LOCAL_SETUP.md#pin-assignments) has their pins. The default is a
Pico 2 W, with TCK, TDO, TDI and TMS on GP0-3. The picture and table below are upstream's Pico
map; this fork's `BOARD_PICO` keeps its JTAG pins but moves the first UART (DBG TX/RX) to
GP0/GP1.

![Pinout image](doc/detailed_pinout.png)


| Pin name | GPIO   | Pico Pin Number |
|:---------|:-------| -          |
| TDI      | GPIO16 | 21         |
| TDO      | GPIO17 | 22         |
| TCK      | GPIO18 | 24         |
| TMS      | GPIO19 | 25         |
| RST      | GPIO20 | 26         |
| TRST     | GPIO21 | 27         |
| DBG TX   | GPIO12 | 16         |
| DBG RX   | GPIO13 | 17         |


## Configuration

Other configurations and RP2040 boards are supported.  

Pick the board when you configure, with `-DDJ_BOARD_TYPE=<name>` (see
[Building](#building-pico-dirtyjtag)), or define a new one in `dirtyJtagConfig.h`. Without
`-DDJ_BOARD_TYPE`, the header's default applies:

``` C
#ifndef BOARD_TYPE
#define BOARD_TYPE BOARD_PICO2_W
#endif
```

The following values control where the code expects to find functionality.  Of course, not all pins can can be used for all functions.  Take care, especially with the UART pins, to ensure compatibility.

``` C
#define PIN_TDI 16 
#define PIN_TDO 17
#define PIN_TCK 18
#define PIN_TMS 19
#define PIN_RST 20
#define PIN_TRST 21

#define LED_INVERTED   0
#define PIN_LED_TX     25
#define PIN_LED_ERROR  25
#define PIN_LED_RX     25

#define CDC_UART_INTF_COUNT 2
#define PIN_UART0 uart0
#define PIN_UART0_TX    12
#define PIN_UART0_RX    13
#define PIN_UART1 uart1
#define PIN_UART1_TX    4
#define PIN_UART1_RX    5
```

If the CDC to UART bridge is not required, or interfers in any way, it can be disabled by setting `CDC_UART_INTF_COUNT` to 0

``` C
#define CDC_UART_INTF_COUNT  0
```
You can define 1 or 2 bridges by setting `CDC_UART_INTF_COUNT` appropriately
``` C
#define CDC_UART_INTF_COUNT 2
```
See the `dirtyJtagConfig.h` file for these and other configuration options.

## Building pico-dirtyJtag

Follow [the instructions](https://github.com/raspberrypi/pico-sdk) for installing the official Raspberry Pi Pico SDK (this fork is built with 2.2.0), point `PICO_SDK_PATH` at it, then clone this repository.

A build takes two choices: the pin map (`DJ_BOARD_TYPE`, a block in `dirtyJtagConfig.h`) and the
SDK board (`PICO_BOARD`), which sets the chip. This fork builds three boards:

| Board                                     | `DJ_BOARD_TYPE`       | `PICO_BOARD`               | Build directory    |
|-------------------------------------------|-----------------------|----------------------------|--------------------|
| Pico 2 W (the default)                    | `BOARD_PICO2_W`       | `pico2_w`                  | `build-pico2w`     |
| RP2040-STAMP on the Colorlight base board | `BOARD_COLORLIGHT_DB` | `solderparty_rp2040_stamp` | `build-colorlight` |
| Pico on the QMTECH RP2040 daughterboard   | `BOARD_PICO`          | `pico`                     | `build`            |

A build directory keeps the board it was first configured with, so give each board its own:

```bash
export PICO_SDK_PATH=/path/to/pico-sdk

# Pico 2 W (the default)
cmake -S . -B build-pico2w -DPICO_BOARD=pico2_w
make -C build-pico2w -j$(nproc)

# RP2040-STAMP on the Colorlight base board
cmake -S . -B build-colorlight -DPICO_BOARD=solderparty_rp2040_stamp -DDJ_BOARD_TYPE=BOARD_COLORLIGHT_DB
make -C build-colorlight -j$(nproc)

# Pico on the QMTECH RP2040 daughterboard
cmake -S . -B build -DPICO_BOARD=pico -DDJ_BOARD_TYPE=BOARD_PICO
make -C build -j$(nproc)
```

If everything succeeds, each directory holds a `dirtyJtag.uf2` that you can directly upload to the
Pico (BOOTSEL), and a `dirtyJtag.elf` for an SWD probe. After the first `cmake`, `make -C <dir>`
alone rebuilds, and remakes a missing or stale UF2.

- The two pin maps this fork added check their chip: built for the wrong `PICO_BOARD`, they stop
  with an `#error` that names the fix.
- Upstream's other pin maps are still in `dirtyJtagConfig.h` (`BOARD_ADAFRUIT_ITSY`,
  `BOARD_SPOKE_RP2040`, `BOARD_WERKZEUG`, `BOARD_QMTECH_RP2040_DAUGHTERBOARD`,
  `BOARD_RP2040_ZERO`) and build the same way, with their own `-DDJ_BOARD_TYPE`. All five built
  with `-DPICO_BOARD=pico` (2026-10-24), but this fork has not run them.
- Two scripts test the build: `./check_uf2_rule.sh build-pico2w` checks that `make` remakes a
  missing or stale UF2, and `./check_board_config.sh` (with `PICO_SDK_PATH` set) checks each of
  our boards' pin map against its chip, in `build-check/`.

[LOCAL_SETUP.md](LOCAL_SETUP.md) has each board's pins, the USB interfaces, and how this setup
flashes and uses them.

## JTAG Usage

Once the board is running `pico-dirtyJtag` and connected to your host you will see a new USB device

```
$ lsusb
...
Bus 003 Device 112: ID 1209:c0ca Generic Jean THOMAS DirtyJTAG
...
```

You can connect to JTAG functionality using `UrJTAG`, `openFPGALoader` or other JTAG tools.  Depending on how your system is set up, you may have to run these commands as root.

For example, using `openFPGALoader`, the following command will connect to the Pico and read the IDCODE from the attached device:

```
> sudo openFPGALoader --cable dirtyJtag --detect
Jtag frequency : requested 6000000Hz -> real 6000000Hz
index 0:
	idcode 0x1112043
	manufacturer lattice
	family ECP5
	model  LFE5UM-45
	irlength 8
```

`UrTag` use is similar:

```
$ sudo jtag

UrJTAG 2021.03 #d9a2943f
Copyright (C) 2002, 2003 ETC s.r.o.
Copyright (C) 2007, 2008, 2009 Kolja Waschk and the respective authors

UrJTAG is free software, covered by the GNU General Public License, and you are
welcome to change it and/or distribute copies of it under certain conditions.
There is absolutely no warranty for UrJTAG.

warning: UrJTAG may damage your hardware!
Type "quit" to exit, "help" for help.

jtag> cable dirtyjtag
jtag> detect
IR length: 8
Chain length: 1
Device Id: 01000001000100010010000001000011 (0x41112043)
  Manufacturer: Lattice Semiconductors (0x043)
  Part(0):      LFE5U-45F-CABGA256 (0x1112)
  Stepping:     0
  Filename:     /usr/local/share/urjtag/lattice/lfe5u-45f-cabga256/lfe5u-45f-cabga256
jtag> 
```

## Debug UART

Once connected, a new port appears as an additional USB interface.  This port can be opened from a terminal emulation program or by custom code.

```
$ ls -la /dev/tty*
...
crw-rw---- 1 root  dialout 166,  0 May 28 08:25 /dev/ttyACM0
...
```
