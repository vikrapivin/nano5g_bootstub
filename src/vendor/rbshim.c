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
/* rbshim.c — the freestanding surface rockbox's file stack expects.
 *
 * The stack (vendor/rb: fat/disk/dir/disk_cache/file/file_internal/
 * fileobj_mgr/rb_namespace/pathfuncs/linked_list/rb-loader/...) is the
 * rockbox bootloader's own code, compiled with its own headers. What it
 * needs from the "kernel" here, in a single-threaded boot with no other
 * tasks and no contention, is semantically the uncontended fast path of
 * the real thing:
 *   - mutex/mrsw primitives -> acquire/release are no-ops (the
 *     bootloader's uncontended mutexes do no work either; nothing can
 *     race: IRQs stay masked for the whole boot);
 *   - panicf -> show a message on the LCD and park (the bootloader
 *     shows the panic on screen and halts);
 *   - errno, the ctype table and the few string helpers newlib would
 *     provide;
 *   - storage_write_sectors resolves to the vendored driver's
 *     BOOTLOADER path, which refuses writes (return -3): the
 *     no-writes guarantee stays by construction.
 * Nothing here invents storage or filesystem behavior.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "config.h"
#include "system.h"
#include "thread.h"   /* IF_PRIO, create_thread proto */

/* memcpy/memset/memmove live in shim.c */
extern void *memcpy(void *dst, const void *src, size_t n);
extern void *memset(void *dst, int c, size_t n);
extern void *memmove(void *dst, const void *src, size_t n);

/* ---- kernel mutex/mrsw/queue/thread: single-threaded, IRQs masked ----
 * storage.c keeps an optional event thread (sleep/spindown housekeeping);
 * the boot never schedules it - it services nothing a read-and-jump boot
 * needs, exactly like a bootloader that never sleeps the disk. */

void mutex_init(void *m) { (void)m; }
void mutex_lock(void *m) { (void)m; }
void mutex_unlock(void *m) { (void)m; }
void mrsw_init(void *l) { (void)l; }
void mrsw_read_acquire(void *l) { (void)l; }
void mrsw_read_release(void *l) { (void)l; }
void mrsw_write_acquire(void *l) { (void)l; }
void mrsw_write_release(void *l) { (void)l; }

/* Signatures from rockbox's thread.h/queue.h (LTO checks them). */
unsigned int create_thread(void (*function)(void), void *stack,
                           size_t stack_size, unsigned flags,
                           const char *name
                           IF_PRIO(, int priority)
                           IF_COP(, unsigned int core));
unsigned int create_thread(void (*function)(void), void *stack,
                           size_t stack_size, unsigned flags,
                           const char *name
                           IF_PRIO(, int priority)
                           IF_COP(, unsigned int core))
{
    (void)function; (void)stack; (void)stack_size; (void)flags; (void)name;
    IF_PRIO((void)priority;)
    IF_COP((void)core;)
    return 1;   /* nonzero id: "thread exists", never scheduled */
}

void queue_init(struct event_queue *q, bool register_queue)
{ (void)q; (void)register_queue; }
void queue_post(struct event_queue *q, long id, intptr_t data)
{ (void)q; (void)id; (void)data; }
void queue_wait_w_tmo(struct event_queue *q, struct queue_event *ev, int ticks)
{ (void)q; (void)ev; (void)ticks; }

/* ---- panicf: show and park ---- */

void nb_panic_halt(const char *msg) __attribute__((noreturn));
void panicf(const char *fmt, ...)
{
    (void)fmt;
    nb_panic_halt("PANIC");
    __builtin_unreachable();
}

/* ---- errno ---- */

static int rbshim_errno;
int *__errno(void) { return &rbshim_errno; }

/* ---- ctype: rockbox's libc expects _ctype_[257] indexed by
 * (_ctype_+1)[(unsigned char)c] (ctype.h macros). Bytes 1..128 cover
 * the ASCII classes; the mirrored sign-extension entries (0 and
 * 129..256) mirror newlib's table layout. Filenames in our path are
 * ASCII; entries follow the class bits of ctype.h. ---- */

#define _U_ 0x01
#define _L_ 0x02
#define _N_ 0x04
#define _S_ 0x08
#define _P_ 0x10
#define _C_ 0x20
#define _X_ 0x40
#define _B_ 0x80

const unsigned char _ctype_[257] = {
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x88, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44,
    0x44, 0x44, 0x44, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x41, 0x41, 0x41, 0x41, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x42, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00,
};

/* ---- string helpers newlib would provide (shim.c has memcpy/memset/
 * memmove); small, standard semantics. ---- */

void *mempcpy(void *dst, const void *src, size_t n)
{
    memcpy(dst, src, n);
    return (char *)dst + n;
}

void *memccpy(void *dst, const void *src, int c, size_t n)
{
    const unsigned char *s = src;
    unsigned char *d = dst;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
        if (s[i] == (unsigned char)c)
            return d + i + 1;
    }
    return NULL;
}

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++)) ;
    return dst;
}

int strncmp(const char *a, const char *b, size_t n)
{
    for (; n--; a++, b++) {
        if (*a != *b) return (unsigned char)*a - (unsigned char)*b;
        if (!*a) break;
    }
    return 0;
}

char *strchr(const char *s, int c)
{
    for (;; s++) {
        if (*s == (char)c) return (char *)s;
        if (!*s) return NULL;
    }
}

char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    for (;; s++) {
        if (*s == (char)c) last = s;
        if (!*s) return (char *)last;
    }
}

int strcasecmp(const char *a, const char *b)
{
    for (; *a || *b; a++, b++) {
        unsigned char ca = (unsigned char)*a, cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return (int)ca - (int)cb;
        if (!ca) break;
    }
    return 0;
}

size_t strlcpy(char *dst, const char *src, size_t n)
{
    size_t len = strlen(src);
    if (n) {
        size_t cp = len < n - 1 ? len : n - 1;
        memcpy(dst, src, cp);
        dst[cp] = 0;
    }
    return len;
}

size_t strlcat(char *dst, const char *src, size_t n)
{
    size_t dl = strlen(dst);
    if (dl >= n) return dl + strlen(src);
    return dl + strlcpy(dst + dl, src, n - dl);
}
