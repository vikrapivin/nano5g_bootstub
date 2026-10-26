@/***************************************************************************
@ *             __________               __   ___.
@ *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
@ *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
@ *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
@ *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__\_ \
@ *                     \/            \/     \/    \/            \/
@ *
@ * Copyright (C) 2026 by Viktor Krapivin
@ *
@ * This program is free software; you can redistribute it and/or
@ * modify it under the terms of the GNU General Public License
@ * as published by the Free Software Foundation; either version 2
@ * of the License, or (at your option) any later version.
@ *
@ * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
@ * KIND, either express or implied.
@ *
@ ****************************************************************************/
@ stub.S — the font-delivered first stage for the nano 5G boot exploit.
@
@ The exploit charstring writes this stub to 0x22050000 and the payload
@ to 0x22060000, then hijacks the CFF decoder's return address into it.
@ The stub's only job: leave the hijacked context cleanly and jump to
@ the payload entry (0x22060001, Thumb), with IRQs off, a private stack,
@ every inherited DMA/interrupt source quiesced, and the MMU/caches torn
@ down to a flat map.
@
@ Build (payload entry 0x22060001):
@   arm-none-eabi-gcc -mcpu=arm1176jzf-s -mthumb -ffreestanding \
@       -nostdlib -nostartfiles [-DSILENT] -c stub.S -o stub.o
@   arm-none-eabi-ld -T stub.ld -o stub.elf stub.o
@   arm-none-eabi-objcopy -O binary stub.elf stub.bin
@
@ SILENT (build with -DSILENT): skip the one green LCD fill at entry.
@ The entry instruction becomes a branch straight to the handoff; every
@ other byte of the image is identical. Without SILENT the stub paints
@ the screen green first (808 B image either way — the fill code stays
@ in the image as dead code so both variants share all addresses).
@
@ Entry contract: the decoder returns into 0x22050001 (Thumb) with the
@ parser's registers live. r0-r6/lr are pushed only so the fill loop
@ can clobber them; the payload must not rely on any register or on the
@ caller's stack.

                .syntax unified
                .thumb

@ ---------------------------------------------------------------- hardware
LCD_STATUS      = 0x3830001c
LCD_CON         = 0x38300000
LCD_WCMD        = 0x38300004
LCD_WDATA       = 0x38300040
LCD_CON_CMD     = 0x01000c20     @ command mode
LCD_CON_PIX     = 0x01100db8     @ data mode
PIX_COUNT       = 0x0002c100     @ one full screen
COLOR_GREEN     = 0x000007e0
COLOR_RED       = 0x0000f800
COLOR_BLUE      = 0x0000001f
SP_PRIVATE      = 0x22068000     @ scratch SRAM above the payload
WDT_BASE        = 0x3c700000
DMA0_BASE       = 0x38200000
DMA1_BASE       = 0x38700000
CLKGATE_BASE    = 0x38e00000
PAYLOAD_ENTRY   = 0x22060001

                .global _start
                .thumb_func
_start:
#ifdef SILENT
                b       seq_green_done
#else
                push    {r0-r6, lr}
#endif

@ one green fill: program the window, then PIX_COUNT data writes
                ldr     r0, =LCD_STATUS
                ldr     r1, =LCD_CON
                ldr     r2, =LCD_WCMD
                ldr     r3, =LCD_WDATA
                bl      wait_lcd
                ldr     r4, =LCD_CON_CMD
                str     r4, [r1]
                bl      wait_lcd
                movs    r4, #0x2a      @ CASET
                str     r4, [r2]
                movs    r4, #0
                bl      wait_lcd
                str     r4, [r3]
                bl      wait_lcd
                str     r4, [r3]
                bl      wait_lcd
                movs    r4, #0xef
                str     r4, [r3]
                bl      wait_lcd
                movs    r4, #0x2b      @ PASET
                str     r4, [r2]
                movs    r4, #0
                bl      wait_lcd
                str     r4, [r3]
                bl      wait_lcd
                str     r4, [r3]
                bl      wait_lcd
                movs    r4, #1
                str     r4, [r3]
                bl      wait_lcd
                movs    r4, #0x77
                str     r4, [r3]
                bl      wait_lcd
                movs    r4, #0x2c      @ RAMWR
                str     r4, [r2]
                bl      wait_lcd
                ldr     r4, =LCD_CON_PIX
                str     r4, [r1]
                ldr     r7, =PIX_COUNT
                ldr     r6, =COLOR_GREEN
pix_loop:
                bl      wait_lcd
                str     r6, [r3]
                subs    r7, #1
                bne     pix_loop
                bl      wait_lcd
                ldr     r4, =LCD_CON_CMD
                str     r4, [r1]
                pop     {r0-r6}
                b       seq_green_done

@ wait for the LCD status busy bit to clear (bit1 = busy)
                .thumb_func
wait_lcd:
                ldr     r4, [r0]
                movs    r5, #18
                ands    r5, r4
                cmp     r5, #2
                bne     wait_lcd
                bx      lr

                .zero   0x30           @ reserved gap (pool alignment)
                .ltorg                  @ literal pool: the 8 registers
                                        @ above + 3 legacy words
                .word   COLOR_RED, COLOR_BLUE, 0x0a408300

@ ---------------------------------------------------------------- handoff
seq_green_done:
                cpsid   if
                ldr     r0, =SP_PRIVATE
                mov     sp, r0
                adr     r3, handoff_arm
                bx      r3
                nop

                .arm
                .align  2
handoff_arm:
@ stop the watchdog (any long payload stage must not be interrupted)
                ldr     r0, =WDT_BASE
                mov     r1, #2
                str     r1, [r0, #4]
                str     r1, [r0, #0x24]
                str     r1, [r0, #0x44]
                str     r1, [r0, #0x64]
                str     r1, [r0, #0xa4]
                str     r1, [r0, #0xc4]
                str     r1, [r0, #0xe4]
                str     r1, [r0, #0x104]
                mvn     r1, #0
                str     r1, [r0, #0x118]
@ quiesce both DMA controllers: 8 channel- enable bits + 2 top enables
                ldr     r4, =DMA0_BASE
                mov     r5, #2
dma_ctl:
                mov     r0, r4
                add     r0, r0, #0x110
                mov     r2, #8
dma_chan:
                ldr     r1, [r0]
                bic     r1, r1, #1
                str     r1, [r0]
                add     r0, r0, #32
                subs    r2, r2, #1
                bne     dma_chan
                add     r0, r4, #48
                ldr     r1, [r0]
                bic     r1, r1, #1
                str     r1, [r0]
                add     r0, r4, #8
                mvn     r1, #0
                str     r1, [r0]
                str     r1, [r0, #8]
                subs    r5, r5, #1
                ldreq   r4, =DMA1_BASE
                bne     dma_ctl
@ gate the remaining clock domains (two banks)
                ldr     r0, =CLKGATE_BASE
                mvn     r1, #0
                str     r1, [r0, #20]
                ldr     r1, [r0, #0xf00]
                add     r0, r0, #0x1000
                mvn     r1, #0
                str     r1, [r0, #20]
                ldr     r1, [r0, #0xf00]
@ caches/MMU off: clean+invalidate D, invalidate I+TLB+BTC, SCTLR M/I/Z/V off
                mov     r0, #0
                mcr     p15, 0, r0, c7, c10, 0
                mcr     p15, 0, r0, c7, c10, 4
                mcr     p15, 0, r0, c7, c5, 0
                mcr     p15, 0, r0, c8, c7, 0
                mcr     p15, 0, r0, c7, c5, 6
                mrc     p15, 0, r0, c1, c0, 0
                bic     r0, r0, #1
                bic     r0, r0, #4
                bic     r0, r0, #0x1000
                bic     r0, r0, #0x800
                mcr     p15, 0, r0, c1, c0, 0
                mcr     p15, 0, r0, c7, c5, 4
@ jump to the payload
                ldr     r0, =PAYLOAD_ENTRY
                bx      r0
                .ltorg                  @ pool: SP, WDT, DMA0/1, CLK, entry

                b       spin_from_arm   @ ARM-mode entry into the spin pad

@ ------------------------------------------------- legacy fills (dead code)
@ The green fill's red/blue siblings from the telemetry development
@ phase. Never reached (the handoff above does not return), kept so the
@ image layout of both variants is byte-identical.

                .thumb
                .thumb_func
fill_red:
                push    {r0-r6, lr}
                ldr     r0, =LCD_STATUS
                ldr     r1, =LCD_CON
                ldr     r2, =LCD_WCMD
                ldr     r3, =LCD_WDATA
                bl      wait_lcd
                ldr     r4, =LCD_CON_CMD
                str     r4, [r1]
                bl      wait_lcd
                movs    r4, #0x2a
                str     r4, [r2]
                movs    r4, #0
                bl      wait_lcd
                str     r4, [r3]
                bl      wait_lcd
                str     r4, [r3]
                bl      wait_lcd
                movs    r4, #0xef
                str     r4, [r3]
                bl      wait_lcd
                movs    r4, #0x2b
                str     r4, [r2]
                movs    r4, #0
                bl      wait_lcd
                str     r4, [r3]
                bl      wait_lcd
                str     r4, [r3]
                bl      wait_lcd
                movs    r4, #1
                str     r4, [r3]
                bl      wait_lcd
                movs    r4, #0x77
                str     r4, [r3]
                bl      wait_lcd
                movs    r4, #0x2c
                str     r4, [r2]
                bl      wait_lcd
                ldr     r4, =LCD_CON_PIX
                str     r4, [r1]
                ldr     r7, =PIX_COUNT
                ldr     r6, =COLOR_RED
pix_loop_red:
                bl      wait_lcd
                str     r6, [r3]
                subs    r7, #1
                bne     pix_loop_red
                bl      wait_lcd
                ldr     r4, =LCD_CON_CMD
                str     r4, [r1]
                pop     {r0-r6, pc}

                .thumb_func
fill_blue:
                push    {r0-r6, lr}
                ldr     r0, =LCD_STATUS
                ldr     r1, =LCD_CON
                ldr     r2, =LCD_WCMD
                ldr     r3, =LCD_WDATA
                bl      wait_lcd
                ldr     r4, =LCD_CON_CMD
                str     r4, [r1]
                bl      wait_lcd
                movs    r4, #0x2a
                str     r4, [r2]
                movs    r4, #0
                bl      wait_lcd
                str     r4, [r3]
                bl      wait_lcd
                str     r4, [r3]
                bl      wait_lcd
                movs    r4, #0xef
                str     r4, [r3]
                bl      wait_lcd
                movs    r4, #0x2b
                str     r4, [r2]
                movs    r4, #0
                bl      wait_lcd
                str     r4, [r3]
                bl      wait_lcd
                str     r4, [r3]
                bl      wait_lcd
                movs    r4, #1
                str     r4, [r3]
                bl      wait_lcd
                movs    r4, #0x77
                str     r4, [r3]
                bl      wait_lcd
                movs    r4, #0x2c
                str     r4, [r2]
                bl      wait_lcd
                ldr     r4, =LCD_CON_PIX
                str     r4, [r1]
                ldr     r7, =PIX_COUNT
                ldr     r6, =COLOR_BLUE
pix_loop_blue:
                bl      wait_lcd
                str     r6, [r3]
                subs    r7, #1
                bne     pix_loop_blue
                bl      wait_lcd
                ldr     r4, =LCD_CON_CMD
                str     r4, [r1]
                pop     {r0-r6, pc}

                .thumb_func
spin:
                b       spin
                .zero   2
                .ltorg                  @ pool: the fills' registers

                .arm
                .align  2
spin_from_arm:
                ldr     ip, [pc]        @ literal follows: spin+1 (0x320)
                bx      ip
                .word   spin+1
                .word   0               @ image ends 0x328 bytes

                .end
