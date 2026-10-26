# Size limits of the boot chain (measured, from the packager data points)

## 1. The bootstub payload itself: hard cap 27,874 bytes

The exploit delivers the payload INSIDE the rsrc font file, as the star
charstring's Type2 chunk encoding. The font slot at MSE 0x93FA00 holds
165,416 bytes total, and the font must stay a working font, so:

    slot 165,416 B  =  base font 78,312 B  +  star charstring
    star charstring =  3 x payload bytes  +  3,480 B fixed
                       (12 font bytes encode every 4 payload bytes,
                        plus the chunk prologue — measured from the
                        packager measurements, linear in between)

    max payload = (165,416 - 78,312 - 3,480) / 3 = 27,874 B

## What the payload contains

The shipped payload is 27,392 B -> 482 B of headroom. It is built -Oz -flto whole-payload with gc-sections and
only the read paths of the file stack compiled in; the write paths are
excluded by construction (-DBOOTLOADER, and NB_FS_READONLY guards the
code unreachable in any boot). Composition:

    FMSS microcode            4,472 B   (fixed cost; pre-assembled in
                                        vendor/fmss-nano5g.h)
    LCD boot font                570 B   (fixed cost; bootfont.c)
    rockbox file stack        ~14 KB     (fat/disk/dir/file/... as
                                        compiled, read paths only)
    the rest                  ~9 KB     (boot sequence, LCD driver,
                                        NAND/FTL driver, shim)

The font builder prints the built size vs the 165,416 B slot, so an
overflow is caught before any write to the device.

## 2. rockbox.ipod: max 8 MiB

load_firmware() is called with MAX_LOADSIZE = 8 * 1024 * 1024 (the
rockbox bootloader's own limit; NB_LOAD_LIMIT). The image loads at
0x08000000 (DRAM_ORIG) — that DRAM belonged to the Apple OS image, which
is dead once the font hijack runs, so the full 8 MiB window is ours.
The bootstub's DRAM .bss (FTL tables + disk cache + FAT buffers,
~494 KB at 0x09000000, the rockbox bootloader's own MOVE_AREA) does not
overlap the window. Current rockbox.ipod: 1.07 MB.

A rockbox.ipod larger than 8 MiB fails in load_firmware with
EFILE_TOO_BIG... check the loader_strerror line on screen ("Cannot load
rockbox.ipod: <reason>").
