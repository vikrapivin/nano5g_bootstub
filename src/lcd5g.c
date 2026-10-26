/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026 by Viktor Krapivin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
/* lcd5g.c — boot-stub LCD text output. See lcd5g.h for the provenance
 * of every register value and the transport contract; nothing here is
 * invented. All waits are bounded by iteration counts (independent of
 * the microsecond timer, so the LCD works even before/if Timer E is
 * up); a timeout latches nb_lcd_fault and later calls become no-ops,
 * exactly like the rockbox nano5g driver's lcd_fault. */
#include <stdint.h>
#include "lcd5g.h"

#define LCDCON   (*(volatile uint32_t *)0x38300000u)
#define LCDWCMD  (*(volatile uint32_t *)0x38300004u)
#define LCDWDATA (*(volatile uint32_t *)0x38300040u)
#define LCDSTATUS (*(volatile uint32_t *)0x3830001cu)

#define LCD_MODE_CMD   0x01000c20u   /* entry-stub literal, device-proven */
#define LCD_MODE_FRAME 0x01100db8u   /* entry-stub literal, device-proven */

/* Enough polls for a settling controller at ~137 MHz; on timeout the
 * wait fails and the caller latches the fault (rockbox uses the same
 * poll-counter-plus-timeout belt, and no-ops once faulted). */
#define LCD_MAX_POLLS 500000u

extern const uint8_t bootfont_bits[95][6];   /* bootfont.c (rockbox sysfont) */

static int lcd_fault;

int nb_lcd_faulted(void)
{
    return lcd_fault;
}

/* Panic entry (rockbox's panicf routes here via rbshim): last-resort
 * message on screen, then park. Bounded: if the LCD is faulted the
 * puts calls no-op and we still park. */
void nb_panic_halt(const char *msg)
{
    nb_lcd_puts(45, "PANIC");
    if (msg)
        nb_lcd_puts(46, msg);
    nb_lcd_puts(47, "MENU+SELECT: reset");
    for (;;)
        __asm volatile ("nop");
}

/* Wait until (status & mask) == value. Returns 0 on timeout. */
static int lcd_wait(uint32_t mask, uint32_t value)
{
    uint32_t polls = LCD_MAX_POLLS;
    while ((LCDSTATUS & mask) != value)
        if (!--polls) {
            lcd_fault = 1;
            return 0;
        }
    return 1;
}

/* Controller mode changes wait for interface idle (b1 set) like the
 * stub's wait_tx and rockbox's s5l_lcd_write_config. */
static int lcd_set_mode(uint32_t mode)
{
    if (lcd_fault || !lcd_wait(0x2u, 0x2u))
        return 0;
    LCDCON = mode;
    return 1;
}

/* One DCS command byte. */
static void lcd_cmd(uint8_t c)
{
    if (lcd_fault || !lcd_wait(0x10u, 0u))
        return;
    LCDWCMD = c;
}

/* One command-mode data byte (a byte per LCDWDATA write, per the
 * reference lcd implementation). */
static void lcd_data8(uint8_t b)
{
    if (lcd_fault || !lcd_wait(0x10u, 0u))
        return;
    LCDWDATA = b;
}

/* One RGB565 pixel in frame mode (rockbox: one write per pixel). */
static void lcd_pixel(uint16_t rgb)
{
    if (lcd_fault || !lcd_wait(0x10u, 0u))
        return;
    LCDWDATA = rgb;
}

/* CASET/PASET window + RAMWR, byte-wise, the stub's exact sequence. */
static void lcd_window(int x0, int y0, int x1, int y1)
{
    lcd_set_mode(LCD_MODE_CMD);
    lcd_cmd(0x2a);
    lcd_data8((uint8_t)(x0 >> 8));
    lcd_data8((uint8_t)x0);
    lcd_data8((uint8_t)(x1 >> 8));
    lcd_data8((uint8_t)x1);
    lcd_cmd(0x2b);
    lcd_data8((uint8_t)(y0 >> 8));
    lcd_data8((uint8_t)y0);
    lcd_data8((uint8_t)(y1 >> 8));
    lcd_data8((uint8_t)y1);
    lcd_cmd(0x2c);
    lcd_set_mode(LCD_MODE_FRAME);
}

/* Fill a rectangle (inclusive coords) with one RGB565 colour. */
static void lcd_fill(int x0, int y0, int x1, int y1, uint16_t color)
{
    int n;
    if (lcd_fault)
        return;
    lcd_window(x0, y0, x1, y1);
    n = (x1 - x0 + 1) * (y1 - y0 + 1);
    while (n--)
        lcd_pixel(color);
    lcd_set_mode(LCD_MODE_CMD);
}

void nb_lcd_init(void)
{
    lcd_fill(0, 0, NB_LCD_WIDTH - 1, NB_LCD_HEIGHT - 1, NB_LCD_BG);
}

/* One 6x8 glyph at pixel (x, y), fg on bg. */
static void lcd_glyph(int x, int y, char c, uint16_t fg, uint16_t bg)
{
    const uint8_t *bits;
    int r, col;
    if (c < 32 || c > 126)
        c = '?';
    bits = bootfont_bits[(uint8_t)c - 32];
    lcd_window(x, y, x + 5, y + 7);
    for (r = 0; r < 8; r++)
        for (col = 0; col < 6; col++)
            lcd_pixel((bits[col] >> r) & 1 ? fg : bg);
    lcd_set_mode(LCD_MODE_CMD);
}

/* Draw s starting at text (row, col) without clearing the line. */
void nb_lcd_puts_at(int row, int col, const char *s)
{
    int x, y;
    if (row < 0 || row >= NB_LCD_ROWS || col < 0)
        return;
    x = col * 6;
    y = row * 8;
    if (lcd_fault)
        return;
    while (*s && x + 6 <= NB_LCD_WIDTH) {
        lcd_glyph(x, y, *s++, NB_LCD_FG, NB_LCD_BG);
        x += 6;
    }
}

void nb_lcd_puts(int row, const char *s)
{
    if (row < 0 || row >= NB_LCD_ROWS)
        return;
    lcd_fill(0, row * 8, NB_LCD_WIDTH - 1, row * 8 + 7, NB_LCD_BG);
    nb_lcd_puts_at(row, 0, s);
}

void nb_lcd_puti(int row, int col, uint32_t v)
{
    char buf[11];
    int i = 10;
    buf[i] = 0;
    do {
        buf[--i] = '0' + (v % 10u);
        v /= 10u;
    } while (v && i);
    nb_lcd_puts_at(row, col, buf + i);
}
