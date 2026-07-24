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
    /* UART1 NOT initialized as CDC bridge — GP4/GP5 are wired to FPGA J12.
     * CDC1 (ttyACM1) is used purely for GPIO control commands. */

    /* Init GP2 as UART MUX select output (active high = swap UART routing) */
    gpio_init(PIN_UART_SEL);
    gpio_set_dir(PIN_UART_SEL, GPIO_OUT);
    gpio_put(PIN_UART_SEL, 0);

    djtag_init();
    while (1) {
        /* Check CDC1 for GPIO control BEFORE jtag_main_task consumes it */
        if (tud_cdc_n_available(1)) {
            uint8_t ch;
            if (tud_cdc_n_read(1, &ch, 1) == 1) {
                if (ch == '1') gpio_put(PIN_UART_SEL, 1);
                else if (ch == '0') gpio_put(PIN_UART_SEL, 0);
            }
        }
        jtag_main_task();
    }
}
