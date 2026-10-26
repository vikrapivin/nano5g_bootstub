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
/* nandboot.h — boot constants and the LCD failure-report stages. */
#ifndef NANDBOOT_H
#define NANDBOOT_H

#include <stdint.h>

#define NB_LOAD_ADDR     0x08000000u   /* rockbox.ipod load address (DRAM) */
#define NB_LOAD_LIMIT    (8u * 1024 * 1024)  /* rockbox bootloader's MAX_LOADSIZE */

enum nb_stage {
    NB_TIMER = 1,       /* usec timer started */
    NB_NAND_INIT = 2,   /* nand_init: physical + FTL R/O */
    NB_FAT_MOUNT = 3,
    NB_FAT_LOOKUP = 4,
    NB_LOAD = 5,
    NB_READY = 6,       /* image verified at NB_LOAD_ADDR */
    NB_JUMPED = 7,
};

enum nb_error {
    NB_E_TIMER = 8,
    NB_E_NAND_INIT = 1,
    NB_E_NO_PARTITION = 2,
    NB_E_FAT_MOUNT = 3,
    NB_E_NO_FILE = 4,
    NB_E_FILE_SIZE = 5,
    NB_E_BAD_MODEL = 6,
    NB_E_CHKSUM = 7,
};

#endif
