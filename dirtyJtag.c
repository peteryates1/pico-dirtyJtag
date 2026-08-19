/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2025 Patrick Dussud
 * Copyright (c) 2021 jeanthom 
 * Copyright (c) 2023 David Williams (davidthings)
 * Copyright (c) 2023 Chandler Klüser
 * Copyright (c) 2024 DangerousPrototypes
 * Copyright (c) 2024 DESKTOP-M9CCUTI\ian
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/pio.h"
#include "pio_jtag.h"
#include "cdc_uart.h"
#include "led.h"
#include "bsp/board.h"
#include "tusb.h"
#include "cmd.h"
#include "get_serial.h"

#include "dirtyJtagConfig.h"

/* GP2 as UART MUX select output for FPGA dual-cluster routing.
 * Directly controllable from host: write '1' or '0' to CDC1 (ttyACM1).
 * Default: low (sel=0). */
#define PIN_UART_SEL 2

void init_pins()
{
    bi_decl(bi_4pins_with_names(PIN_TCK, "TCK", PIN_TDI, "TDI", PIN_TDO, "TDO", PIN_TMS, "TMS"));
    #if !( BOARD_TYPE == BOARD_QMTECH_RP2040_DAUGHTERBOARD )
    bi_decl(bi_2pins_with_names(PIN_RST, "RST", PIN_TRST, "TRST"));
    #endif
}

pio_jtag_inst_t jtag = {
            .pio = pio0,
            .sm = 0
};

void djtag_init()
{
    init_pins();
    #if !( BOARD_TYPE == BOARD_QMTECH_RP2040_DAUGHTERBOARD )
    init_jtag(&jtag, 1000, PIN_TCK, PIN_TDI, PIN_TDO, PIN_TMS, PIN_RST, PIN_TRST);
    #else
    init_jtag(&jtag, 1000, PIN_TCK, PIN_TDI, PIN_TDO, PIN_TMS, 255, 255);
    #endif
}
#define CMD_BUF_SIZE 1024
static uint8_t cmd_buf[CMD_BUF_SIZE];
static uint32_t cmd_buf_len = 0;
static uint8_t tx_buf[512];

void jtag_main_task()
{
    tud_task();

    if (tud_vendor_available())
    {
        led_rx( 1 );
        uint32_t n = tud_vendor_read(cmd_buf + cmd_buf_len, CMD_BUF_SIZE - cmd_buf_len);
        cmd_buf_len += n;
        led_rx( 0 );
    }
#if ( CDC_UART_INTF_COUNT > 0 )
    else
    {
        cdc_uart_task();
    }
#endif

    if (cmd_buf_len > 0)
    {
        uint32_t consumed = cmd_handle(&jtag, cmd_buf, cmd_buf_len, tx_buf);
        if (consumed > 0)
        {
            cmd_buf_len -= consumed;
            if (cmd_buf_len > 0)
                memmove(cmd_buf, cmd_buf + consumed, cmd_buf_len);
        }
    }
}

void jtag_task()
{
    tud_task();
    if (tud_vendor_available() && cmd_buf_len < CMD_BUF_SIZE)
    {
        uint32_t n = tud_vendor_read(cmd_buf + cmd_buf_len, CMD_BUF_SIZE - cmd_buf_len);
        cmd_buf_len += n;
    }
}


//this is to work around the fact that tinyUSB does not handle setup request automatically
//Hence this boiler plate code
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
    if (stage != CONTROL_STAGE_SETUP) return true;
    return false;
}

int main()
{
    board_init();
    usb_serial_init();
    tusb_init();

    led_init( LED_INVERTED, PIN_LED_TX, PIN_LED_RX, PIN_LED_ERROR );
#if ( CDC_UART_INTF_COUNT > 0 )
    cdc_uart_init( 0, PIN_UART0, PIN_UART0_RX, PIN_UART0_TX );
#endif
#if ( CDC_UART_INTF_COUNT > 1 )
    /* uart1 was declared but never initialised. CDC_UART_INTF_COUNT 2 creates
       the second USB CDC interface, so the host enumerates a second ttyACM and
       opens it happily -- but without this call gpio4/5 never get
       GPIO_FUNC_UART, uart1 is never uart_init'ed and no DMA is set up, so the
       port is connected to nothing. It presents as total silence on a port that
       looks present and healthy, which is indistinguishable from bad wiring or
       a dead FPGA design; it cost a loopback jig looping four pin pairs to rule
       those out. Everything else already supports two: uart_devices[2] is sized
       for it and every service loop runs to CDC_UART_INTF_COUNT. */
    cdc_uart_init( 1, PIN_UART1, PIN_UART1_RX, PIN_UART1_TX );
#endif

    /* UART MUX select REMOVED. It claimed GP2 as a GPIO output, but GP2 is JTAG
       TMS in this board config and djtag_init() below hands GP2 to the PIO
       immediately afterwards -- so the gpio_put calls never reached a pin. Its
       only surviving effect was harmful: the control channel stole every byte
       written to CDC1 (see the loop below), so the second UART could not work
       even once it was initialised. */

    djtag_init();
    while (1) {
        /* CDC1 is a UART bridge, not a control channel. Reading it here consumed
           the host's bytes before cdc_uart_task could forward them to uart1,
           which made ttyACM2 look permanently dead: the port enumerated, opened
           cleanly and swallowed everything. */
        jtag_main_task();
    }
}
