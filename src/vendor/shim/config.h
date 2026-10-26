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
/* nandboot freestanding shim of rockbox config.h.
 * Only what the vendored nano 5G NAND/FTL read-only sources need:
 * the target identity, BOOTLOADER (which compiles out all FTL/nand write
 * paths in the rockbox sources) and the numeric CONFIG_CPU ids that
 * s5l87xx.h switches on. */
#ifndef NANDBOOT_SHIM_CONFIG_H
#define NANDBOOT_SHIM_CONFIG_H

#define S5L8700 8700
#define S5L8701 8701
#define S5L8702 8702
#define S5L8720 8720
#define S5L8730 8730

#define CONFIG_CPU S5L8730
#define IPOD_NANO5G 1

/* Read-only driver: the rockbox sources keep every program/erase path
 * behind #ifndef BOOTLOADER. */
#define BOOTLOADER 1

/* Not defined (features we do not build): HAVE_STORAGE_FLUSH,
 * HAVE_STORAGE_READONLY, STORAGE_GET_INFO, HAVE_MULTIDRIVE. */

#endif
