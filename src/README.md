# nano5g bootstub — the silent Rockbox boot payload

Bootstub = the boot-time payload of the untethered chain. It is written
by the font exploit to **0x22060000** and entered at **0x22060001**
(Thumb) by the entry stub, with IRQs as OSOS left them. It never talks
USB, has no console, and does not return.

Boot sequence (nb_boot_main, mirrors the Rockbox bootloader's main()
order exactly):

    watchdog off (WDTCON = 0xa5a50000)
    timer probe (guarded, for the bounded delays)
    storage_init    -> FMSS/FMC physical NAND, read-only
    filesystem_init -> FTL + rockbox file stack
    disk_mount_all  -> MBR + FAT32 (media partition)
    load_firmware("/.rockbox/rockbox.ipod", 0x08000000)
    jump to Rockbox

On failure it prints the loader error on the LCD (rockbox bootloader
style) and parks: "Boot stopped. MENU+SELECT: reset".

## Build

```sh
make          # -> build/bootstub.bin (raw image) + bootstub.elf/.dis/.map
```

Nothing but `arm-none-eabi-gcc` + `make` is required. The build is
deterministic (fixed sources, no timestamps): same toolchain in, same
bytes out. `make` also assembles the entry stub (stub/stub.S) into
`build/stub.bin` — 808 B, silent variant (no green entry fill; build
the same source without -DSILENT for the fill).

Memory map: load/entry 0x22060000, DRAM bss ~201 KB at 0x09000000
(FTL tables; NOLOAD). rockbox.ipod is loaded to 0x08000000 (cap 8 MiB).
See ../SIZE-LIMITS.md for the payload cap math (27,874 B max).

## Layout

    entry.S            entry: watchdog off, stack, -> nb_boot_main (ARM)
    nandboot.c/h        boot sequence + LCD boot messages
    lcd5g.c/h + bootfont.c   the LCD driver (device-proven register
                       sequences) and a 95-glyph 4x6 boot font
    vendor/nand-nano5g.c   the physical FMSS/FMC NAND driver (read-only
                       under -DBOOTLOADER; compiles exactly like the
                       Rockbox bootloader build does)
    vendor/ftl.c          the logical FTL (read-only)
    vendor/read.s, readid.s   the FMSS microcode sources, assembled at
                       build time by tools/fmssasm.c into the
                       fmss-nano5g.h the driver includes
    vendor/shim.c, shim/   the freestanding surface the driver needs
                       (udelay, panic, logf stubs)
    vendor/buildconf/  OUR replacements for the two Rockbox
                       build-generated headers the vendored sources
                       include (autoconf.h: the arch/endian/threading/
                       directory defines of a nano5G bootloader build;
                       sysfont.h: the stock system font metrics). No
                       configure-generated files ship in this tree.
    vendor/rbshim.c    the freestanding surface the rockbox file stack
                       needs (no-op mutex/queue, errno, ctype,
                       panicf -> LCD halt)
    vendor/rb/         the Rockbox file stack, upstream source with the
                       preprocessor conditionals resolved for this build
                       (dead branches removed by tools/flatten.py; live
                       guards kept; config.h keeps its upstream form as
                       the target switchboard)

## Design rules

  * Read-only by construction: `nand_write_sectors` returns -3 under
    -DBOOTLOADER — the same flag the Rockbox bootloader uses.
  * The Rockbox sources under vendor/rb/ keep their upstream license
    (GPL-2.0+). The tree is pruned to exactly the files this build
    compiles/preprocesses, and the preprocessor conditionals are
    resolved for this build (dead branches removed by tools/flatten.py,
    live guards kept verbatim; config.h keeps its upstream form as the
    target switchboard).
  * NB_FS_READONLY guards only code unreachable in any boot (e.g.
    ftruncate); the R/O property comes from the build flags.
