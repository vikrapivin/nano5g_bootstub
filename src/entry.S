@**************************************************************************
 @             __________               __   ___.
 @   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 @   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 @   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 @   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__\_ \
 @                     \/            \/     \/    \/            \/
 @
 @ Copyright (C) 2026 by Viktor Krapivin
 @
 @ This program is free software; you can redistribute it and/or
 @ modify it under the terms of the GNU General Public License
 @ as published by the Free Software Foundation; either version 2
 @ of the License, or (at your option) any later version.
 @
 @ This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 @ KIND, either express or implied.
 @
 @***************************************************************************/
@
/* Silent NAND-boot entry: no USB, no LCD, no console.
 * Loaded at its linked address, entered in Thumb state with privileged CPU
 * mode and the loader's MMU/cache setup intact. IRQ and FIQ stay masked.
 */
.syntax unified
.cpu arm1176jzf-s
.section .text.entry, "ax", %progbits
.thumb
.align 2
.global _start
.type _start, %function
.thumb_func
_start:
    cpsid if
    /* Watchdog off (device-proven: this payload's init writes 0xa5 to
     * 0x3c800000); the FTL scan can take a few seconds and must not be interrupted. */
    ldr r0, =0x3c800000
    movs r1, #0xa5
    str r1, [r0]
    ldr r0, =__stack_top
    mov sp, r0

    /* Raw .bin contains initialized data, but NOLOAD BSS must be cleared.
     * The linker aligns both bounds to whole cache lines. */
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    movs r2, #0
1:
    cmp r0, r1
    bhs 2f
    stmia r0!, {r2}
    b 1b
2:
    /* A symbol-derived pointer preserves the ARM/Thumb interworking bit. */
    ldr r3, =nb_boot_main
    blx r3
3:
    b 3b
.size _start, .-_start
.ltorg
.section .note.GNU-stack, "", %progbits
