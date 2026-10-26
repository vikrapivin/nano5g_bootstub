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
/* nandboot.c — the silent untethered Rockbox boot payload.
 * Same font-exploit delivery as the USB console launcher variant, but
 * this payload has NO USB stack and NO
 * console: it boots straight into rockbox.ipod through the rockbox
 * nano 5G read-only NAND/FTL stack.
 *
 * Stages (tracked for the on-screen failure report; the payload has
 * no other channel):
 *   N1  timer + nand_init()      — physical layer, READ ID, FTL R/O mount
 *   N2  FAT32 mount + lookup     — /.rockbox/rockbox.ipod
 *   N3  file stream to 0x08000000 + scramble checksum
 *   N4  cache flush + ARM jump   — rockbox's crt0 takes it from there
 *
 * The rockbox.ipod format is the tools/scramble "add" format: an 8-byte
 * header (big-endian u32 checksum, then the 4-byte model id "nn5g")
 * followed by the raw image. The image links at 0x08000000 and its
 * first word is an ARM branch (the same crt0 the rockbox bootloader
 * uses, which disables MMU/caches itself); the rockbox bootloader
 * enters it with IRQs masked and caches flushed, MMU on its flat map —
 * the OSOS context we inherit is equivalent.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "nandboot.h"
#include "lcd5g.h"
#include "clocking-s5l8702.h"   /* rockbox's own: USEC_TIMER, usec_timer_init */
/* The rockbox bootloader's own load path, vendored under vendor/rb
 * (byte-identical sources): storage_init -> filesystem_init ->
 * disk_mount_all -> load_firmware, exactly bootloader/ipodnano5g.c. */
#include "config.h"
#include "system.h"        /* the stack's own include order: the rb
                              headers expect the system.h macro chain */
#include "storage.h"
#include "file_internal.h"
#include "disk.h"
#include "rb-loader.h"
#include "loader_strerror.h"
int usec_timer_running(void);

/* vendor storage entry points */
int nand_init(void);
void usec_timer_init(void);

/* ---- boot state for the on-screen failure report ---- */

static uint32_t nb_stage;      /* enum nb_stage reached */
static int nb_fail_rc;         /* rc of the failed operation */

static void report_stage(enum nb_stage s)
{
    nb_stage = (uint32_t)s;
}

static int fail(int stage, int code)
{
    nb_stage = (uint32_t)stage;
    nb_fail_rc = code;
    return code;
}

/* ---- rockbox.ipod checksum (tools/scramble "add" format) ---- */

/* ---- the boot attempt (N1..N3); returns 0 when rockbox.ipod sits
 * verified at 0x08000000, negative otherwise. ---- */

/* The boot attempt: the rockbox bootloader's main() sequence, verbatim
 * order (bootloader/ipodnano5g.c): timer -> storage_init() ->
 * filesystem_init() -> disk_mount_all() -> load_firmware() -> jump.
 * Every failure keeps the rockbox error line on screen and parks. */
int nb_prepare(void)
{
    int rc;

    /* N1: microsecond timer (driver timeouts). The OSOS context may
     * already have Timer E running (the Apple EFI chain used it); probe
     * first and only poke the clock tree if it is frozen. */
    report_stage(NB_TIMER);
    if (!usec_timer_running()) {
        usec_timer_init();
        if (!usec_timer_running())
            return fail(NB_TIMER, NB_E_TIMER);   /* clean: no hang */
    }

    /* bootloader: "Opening storage..." */
    report_stage(NB_NAND_INIT);
    nb_lcd_puts(2, "Opening storage...");
    rc = storage_init();
    if (rc)
        return fail(NB_NAND_INIT, rc);

    /* bootloader: filesystem_init(); disk_mount_all() */
    filesystem_init();
    report_stage(NB_FAT_MOUNT);
    if (disk_mount_all() <= 0)
        return fail(NB_FAT_MOUNT, -1);

    /* bootloader: "Loading /.rockbox/" BOOTFILE */
    report_stage(NB_FAT_LOOKUP);
    nb_lcd_puts(3, "Loading /.rockbox/" BOOTFILE);
    rc = load_firmware((unsigned char *)NB_LOAD_ADDR,
                       BOOTDIR "/" BOOTFILE, (int)NB_LOAD_LIMIT);
    if (rc <= EFILE_EMPTY)
        return fail(NB_LOAD, rc);

    /* rockbox's loader verified the model + checksum and left the
     * executable image at NB_LOAD_ADDR. */
    report_stage(NB_READY);
    return 0;
}

/* ---- N4: the handoff (never returns) ---- */

void __attribute__((noreturn, naked)) nb_jump(uint32_t entry)
{
    (void)entry;
    __asm__ volatile(
        "mov r4, r0                         \n"  /* entry */
        "cpsid if                           \n"  /* IRQ/FIQ masked (already true) */
        "mov r0, #0                         \n"
        "mcr p15, 0, r0, c7, c14, 0         \n"  /* clean + invalidate dcache */
        "mcr p15, 0, r0, c7, c10, 4         \n"  /* drain write buffer */
        ".rept 4                            \n"  /* ARM1176 erratum 411920 */
        "mcr p15, 0, r0, c7, c5, 0          \n"  /* invalidate icache + BTB */
        ".endr                              \n"
        ".rept 11                           \n"
        "nop                                \n"
        ".endr                              \n"
        "mcr p15, 0, r0, c7, c5, 4          \n"  /* flush prefetch buffer */
        "bx r4                              \n"
        "1: b 1b                            \n");
}

/* ---- the silent boot: auto-run at power-on, no USB, no console ---- */

void nb_boot_main(void)
{
    /* Rockbox-bootloader look: black screen, white sysfont text, the
     * same title/progress/error lines the DFU bootloader shows. LCD
     * writes are bounded and fault-latching, so this works (or quietly
     * no-ops) no matter what state the panel is in. */
    nb_lcd_init();
    nb_lcd_puts(0, "Rockbox boot loader");
    nb_lcd_puts(1, "nano5g bootstub");

    if (nb_prepare() == 0) {
        nb_lcd_puts(4, "Starting Rockbox...");
        nb_jump(NB_LOAD_ADDR);     /* never returns */
    }

    /* Boot failed: show the rockbox error line for the failed stage,
     * then park. There is no console state; power-cycle to retry, or
     * flash the console launcher variant to debug over USB. */
    switch (nb_stage) {
    case NB_TIMER:
        nb_lcd_puts(5, "Timer error");
        nb_lcd_puti(5, 12, nb_fail_rc);
        break;
    case NB_NAND_INIT:
        nb_lcd_puts(5, "Storage error:");   /* rockbox line */
        nb_lcd_puti(5, 15, nb_fail_rc);
        break;
    case NB_FAT_MOUNT:
        nb_lcd_puts(5, "No FAT partition found");   /* rockbox line */
        break;
    case NB_FAT_LOOKUP:
    case NB_LOAD:
        /* rockbox line: "Cannot load <file>: <reason>" */
        nb_lcd_puts(5, "Cannot load " BOOTFILE ":");
        nb_lcd_puts(6, loader_strerror(nb_fail_rc));
        break;
    default:
        nb_lcd_puts(5, "Boot error");
        nb_lcd_puti(5, 11, nb_fail_rc);
        break;
    }
    nb_lcd_puts(8, "Boot stopped. MENU+SELECT: reset");  /* rockbox line */
    for (;;)
        __asm volatile ("nop");
}
