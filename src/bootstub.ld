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
/* nandboot image layout. Code, data and the stack stay inside the
 * exploit payload window in SRAM (0x22060000..0x22070000); the big read-only FTL/NAND work tables go
 * to DRAM at 0x09000000 — the address the rockbox bootloader itself
 * relocates to, well above the live Apple OS image (ends ~0x086cfb9c)
 * and its early-boot heap usage, and disjoint from the rockbox.ipod
 * load address 0x08000000.
 *
 * Entry contract unchanged: loaded at its linked address, entered in
 * Thumb at _start with IRQ/FIQ masked. entry.S clears the DRAM .bss.
 */
OUTPUT_FORMAT(elf32-littlearm)
OUTPUT_ARCH(arm)
ENTRY(_start)

__load_address = DEFINED(LAUNCHER_BASE) ? LAUNCHER_BASE : 0x22060000;
__ram_bytes = DEFINED(LAUNCHER_RAM_BYTES) ? LAUNCHER_RAM_BYTES : 0x10000;
__stack_bytes = 0x2000;
__dram_bss_base = 0x09000000;
__dram_bss_bytes = 4M;
MEMORY {
    PAYLOAD (rwx) : ORIGIN = __load_address, LENGTH = __ram_bytes
    DRAMBSS (rw)  : ORIGIN = __dram_bss_base, LENGTH = __dram_bss_bytes
}

SECTIONS
{
    .text : ALIGN(32) {
        KEEP(*(.text.entry))
        *(.text*) *(.glue_7*) *(.vfp11_veneer)
        *(.rodata*)
    } > PAYLOAD
    .data : ALIGN(32) {
        *(.data*)
        . = ALIGN(32);
        __file_end = .;
    } > PAYLOAD
    .bss (NOLOAD) : ALIGN(32) {
        __bss_start = .;
        *(.bss*) *(COMMON) *(.ibss*)   /* orphan .ibss zero-init data would
                                         otherwise ship as payload zeros */
        . = ALIGN(32);
        __bss_end = .;
    } > DRAMBSS
    .stack (NOLOAD) : ALIGN(32) {
        __stack_bottom = .;
        . += __stack_bytes;
        __stack_top = .;
        __runtime_end = .;
    } > PAYLOAD
    /DISCARD/ : { *(.comment) *(.note*) *(.ARM.exidx*) *(.ARM.extab*) *(.eh_frame*) }
    ASSERT((__load_address & 31) == 0, "load address must be 32-byte aligned")
    ASSERT(__file_end - __load_address <= 120 * 1024, "raw image exceeds 120 KiB")
    ASSERT(__bss_end - __bss_start <= __dram_bss_bytes, "DRAM BSS overflow")
    ASSERT(__stack_top - __load_address <= __ram_bytes, "image+stack exceeds SRAM window")
}
