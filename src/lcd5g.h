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
/* lcd5g.h — boot-stub LCD text output (Rockbox bootloader look).
 *
 * Minimal PIO path over the S5L8730 LCD controller (0x38300000) using
 * the device-proven sequences only:
 *   - the entry stub's device-proven green band (command mode 0x01000c20,
 *     frame mode 0x01100db8, CASET/PASET/RAMWR, LCDSTATUS 0x1c polled
 *     with b4 (TX busy) clear before every write) — observed on device
 *     in the OSOS context this payload runs in;
 *   - the rockbox nano5g driver contract (one RGB565 halfword per
 *     LCDWDATA write in frame mode; waits bounded by a poll counter,
 *     a timeout latches a fault and later calls become no-ops) — the
 *     DFU bootloader draws text with it on device.
 *
 * No panel init, no backlight change: the Apple OS context already has
 * the panel awake and lit; this code only writes GRAM. Text uses the
 * rockbox sysfont (bootfont.c), white on black, like the bootloader.
 */
#ifndef NB_LCD5G_H
#define NB_LCD5G_H

#include <stdint.h>

/* Screen: 240 x 376 RGB565. Text grid: 6x8 font -> 40 cols, 47 rows. */
#define NB_LCD_WIDTH  240
#define NB_LCD_HEIGHT 376
#define NB_LCD_COLS   (NB_LCD_WIDTH / 6)
#define NB_LCD_ROWS   (NB_LCD_HEIGHT / 8)

/* Rockbox bootloader colours: white foreground on black. */
#define NB_LCD_FG 0xffffu
#define NB_LCD_BG 0x0000u

/* Clear the whole screen to black. Bounded; never hangs. */
void nb_lcd_init(void);

/* Draw s at text row (0..NB_LCD_ROWS-1), column 0, white on black.
 * The line is cleared first, so re-putting a row replaces it. */
void nb_lcd_puts(int row, const char *s);

/* Same, starting at column col, without clearing the line. */
void nb_lcd_puts_at(int row, int col, const char *s);

/* Append a decimal number at column col of row (0..9 digits). */
void nb_lcd_puti(int row, int col, uint32_t v);

/* True if the LCD fault latch tripped (a wait timed out). */
int nb_lcd_faulted(void);

#endif /* NB_LCD5G_H */
