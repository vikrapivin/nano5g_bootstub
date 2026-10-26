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
/* nandboot shim.c — freestanding implementations of the rockbox
 * infrastructure the vendored nano 5G NAND/FTL read-only sources use.
 *
 * Sources of the values (the rockbox nano5g port):
 *  - clockgate_enable: firmware/target/arm/s5l8702/clocking-s5l8702.c
 *    (PWRCON(i): CLK_BASE + 0x44 + 4*i, bit = 1 << (gate & 31)).
 *  - usec_timer_init: same file, the S5L8730 branch (CG16_RTIME on OSC0,
 *    CLOCKGATE_TIMERE(56)/TIMERE_2(130), timer E at 1 MHz from the
 *    24 MHz crystal, S5L8702_OSC0_HZ = 24000000 under IPOD_NANO5G).
 *  - cache range ops: the cpucache-arm.h prototypes, ARM1176 (32-byte
 *    lines, MVA-based CP15 ops).
 * The payload is single-threaded; udelay busy-waits on the microsecond
 * timer the same timer init starts.
 */
#include "shim/config.h"
#include "shim/cpu.h"
#include "shim/clocking-s5l8702.h"
#include <stdint.h>
#include <stddef.h>

long current_tick;

int usec_timer_running(void)
{
    uint32_t a = USEC_TIMER;
    for (volatile int i = 0; i < 20000; i++)
        ;
    uint32_t b = USEC_TIMER;
    return b != a;
}

/* ---- cache maintenance (ARM1176, 32-byte lines) ---- */

/* One of the three MVA-based line ops; crm is a literal (MCR requires
 * an immediate CRm): 10 clean, 14 clean+invalidate, 6 invalidate. */
#define CACHE_RANGE(name, crm)                                          \
static void name(const void *base, unsigned int size)                    \
{                                                                       \
    uint32_t addr = (uint32_t)base & ~31u;                               \
    uint32_t end = ((uint32_t)base + size + 31u) & ~31u;                 \
    for (; addr < end; addr += 32)                                       \
        __asm__ volatile ("mcr p15, 0, %0, c7, c" #crm ", 1"            \
                          : : "r"(addr));                                \
    __asm__ volatile ("mcr p15, 0, %0, c7, c10, 4" : : "r"(0));          \
}

CACHE_RANGE(clean_lines, 10)          /* c7, c10, 1: clean by MVA */
CACHE_RANGE(clean_inv_lines, 14)      /* c7, c14, 1: clean + invalidate */
CACHE_RANGE(inv_lines, 6)             /* c7, c6, 1: invalidate by MVA */

void commit_dcache_range(const void *base, unsigned int size)
{ clean_lines(base, size); }

void commit_discard_dcache_range(const void *base, unsigned int size)
{ clean_inv_lines(base, size); }

void discard_dcache_range(const void *base, unsigned int size)
{ inv_lines(base, size); }

/* ---- freestanding libc ---- */

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    unsigned char *d = dst;
    while (n--) *d++ = (unsigned char)c;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a, *y = b;
    for (; n--; x++, y++)
        if (*x != *y) return *x - *y;
    return 0;
}
