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
/* nandboot freestanding shim of rockbox clocking-s5l8702.h: the nano 5G
 * crystal constants and the clock/timer service declarations. The 5G
 * values come from the rockbox clocking header's IPOD_NANO5G branch. */
#ifndef NANDBOOT_SHIM_CLOCKING_H
#define NANDBOOT_SHIM_CLOCKING_H

#include <stdbool.h>

#define S5L8702_OSC0_HZ   24000000  /* crystal, PLL0 input (nano 5G) */
#define S5L8702_OSC1_HZ   32768

void clockgate_enable(int gate, bool enable);
void usec_timer_init(void);

#endif
