# iPod nano 5G (S5L8730) — untethered Rockbox bootstub

The silent boot payload for the nano 5G boot-time exploit chain. At every
power-on, with no USB and no button presses, the device boots straight into
Rockbox:

    Apple boot -> font engine parses the resource CinecavXMono -> the exploit
    charstring ("star") writes a stub to 0x22050000 and this payload to
    0x22060000 and hijacks the decoder's return -> the stub hands off ->
    payload: watchdog off, storage init (read-only), filesystem init,
    disk mount, load /.rockbox/rockbox.ipod, jump to 0x08000000.

Why the patched resource image even boots: the device's own FirmwareStatus
flag (bit1) makes the boot loader skip rsrc signature validation — see
`PRIV-PRVD.md`.

## The two images

The exploit delivers exactly two blobs, both embedded in the star
charstring and written by it to fixed SRAM addresses before it hijacks
the CFF decoder's return address:

  * **the entry stub** — `build/stub.bin`, 808 B at 0x22050000.
    The first stage. It is entered DIRECTLY from the hijacked parser
    context (the corrupted FreeType call frame), so its one job is to
    leave that fragile state cleanly: IRQs off, its own stack, the
    DMA controllers / clock gates the parser left running quiesced,
    caches and MMU torn down to a flat map — then jump to the payload
    at 0x22060001 (Thumb). It deliberately does nothing else: no LCD,
    no NAND, no console, because it runs inside a corrupted decoder
    frame where almost nothing can be trusted. It is small,
    position-fixed hand-written assembly (src/stub/stub.S + stub.ld;
    built with -DSILENT so the one green telemetry fill is skipped).
  * **the payload** — `build/bootstub.bin`, ~27.4 KB at 0x22060000.
    The actual boot program. It runs in the clean flat-map state the
    stub prepared: watchdog off, microsecond timer, read-only NAND +
    FTL init, the rockbox file stack, FAT mount, load and verify
    /.rockbox/rockbox.ipod, then a plain ARM jump into Rockbox. All the
    maintainable C lives here.

They are built as two separate images (stub.S/stub.ld vs. the C tree +
bootstub.ld) because their worlds are different: the stub is a
minimal, byte-conscious teardown sequence tuned to the hijack site,
while the payload is ordinary freestanding C. Both share the font's
encoding budget — every payload byte costs three font bytes — which is
why the stub stays as small as it is.

## The full build pipeline

### 0. Prerequisites

  * `arm-none-eabi-gcc` + `make` — the only toolchain
  * a host `cc` (it builds the FMSS microcode assembler)
  * `python3` + fontTools (`pip install -r font/requirements.txt`) —
    needed for the packaging step only
  * **your own** `Firmware.MSE`: download the iPod nano 5G restore IPSW
    `iPod_1.0.2_34A20020.ipsw`, unzip it, take `Firmware.MSE`
    (136,785,920 B, sha1 910222822725573d8c0b7c7e399aedef17c118b8).
    Apple content, copyrighted, not part of this tree.

### 1. Build the payload and the entry stub

    cd src && make

    build/fmssasm          <- host cc compiles src/tools/fmssasm.c
    build/fmss-nano5g.h    <- fmssasm assembles vendor/readid.s +
                              vendor/read.s (the FMSS sequencer
                              microcode) at build time
    build/bootstub.bin     <- the payload (~27.4 KB):
         entry.S        ARM; watchdog off, stack, -> nb_boot_main
         nandboot.c     the boot sequence (rockbox bootloader order)
         lcd5g.c,       the LCD text output + its 95-glyph font
         bootfont.c     (Thumb)
         vendor/shim.c, the freestanding surface the driver and the
         vendor/rbshim.c rockbox file stack need (no OS, no libc)
         vendor/nand-nano5g.c, vendor/ftl.c
                        the read-only NAND driver + logical FTL
         vendor/rb/**   the rockbox file stack (upstream source with
                        the conditionals resolved for this build;
                        vendor/buildconf/ replaces the generated
                        autoconf.h + sysfont.h)
         linked by bootstub.ld: load 0x22060000, DRAM bss 0x09000000,
         gc-sections, zero undefined symbols
    build/stub.bin        <- src/stub/stub.S assembled with -DSILENT
                             (via stub/stub.ld at 0x22050000), 808 B

The build is deterministic (fixed sources, no timestamps): same
toolchain in, same bytes out.

### 2. Package the exploit font

    cd font && python3 make_exploit_font.py --mse Firmware.MSE \
        --stub ../src/build/stub.bin \
        --payload ../src/build/bootstub.bin \
        --payload-addr 0x22060000 \
        --out exploit_font.otf \
        --patch-mse Firmware.patched.MSE      (optional convenience)

What this does: takes the stock CinecavXMono out of your MSE as the
container, the stock Helvetica's name table, and builds the star
charstring into glyph `space` — the Type2 program that writes the stub
to 0x22050000, the payload to 0x22060000, and hijacks the CFF decoder's
return address into the stub. The build is deterministic (fixed
timestamp and checksum constant) and the printed size is checked
against the 165,416-byte font slot.

### 3. Write to the device

The write primitive ships as the sibling `emCORE/` directory —
`emCORE/UPLOAD-BOOTSTUB.md` is the step-by-step procedure (DFU load of
the emCORE image, FTL-backed sector write, FirmwareStatus check,
rollback). In short: device raw LBA 2431..2472, intra-sector offset 0xA00,
42 sectors, the built font zero-padded to the 165,416-byte slot.

The only edit the firmware image needs:

    fw partition + 0x93FA00, the built font zero-padded to
    165,416 bytes (the rsrc "Helvetica" slot) — or equivalently the
    patched Firmware.patched.MSE region. Nothing else in the image
    changes.

The device loads the patched font because of its own PRIV gate
(PRIV-PRVD.md): FirmwareStatus bit1 in the private partition makes Bds
skip rsrc signature validation — a state Apple's own OS maintains.

### 4. What happens on the device

    cold boot -> rsrc font loads unvalidated (bit1) -> FreeType renders
    the star -> stub written to 0x22050000, payload to 0x22060000, LR
    hijack -> stub handoff (IRQs off, private stack, DMA/clock gates
    quiesced, MMU/caches torn down to a flat map) -> payload: watchdog
    off -> timer -> storage_init -> filesystem_init -> disk_mount_all
    -> load /.rockbox/rockbox.ipod -> 0x08000000 -> jump to Rockbox.

Requirements on the disk: `/.rockbox/rockbox.ipod` on the media
partition, at most 8 MiB. The FTL logical scan inside storage_init can
take a few seconds on this NAND; the payload disables the watchdog
first precisely so nothing interrupts that scan — don't power the
device off while the boot lines are on screen. On any failure the LCD
shows the rockbox-style error line for the failed stage and parks
("Boot stopped. MENU+SELECT: reset").

## Size caps

The payload must stay <= 27,874 bytes (font-slot math, SIZE-LIMITS.md);
the current build has ~500 B of headroom and the font builder rejects
an overflow before any write to the device.

## Licensing

Our code is GPL-2.0+. The file stack under `src/vendor/rb/` is upstream
Rockbox source (GPL-2.0+) with the preprocessor conditionals resolved
for THIS build: dead branches (other targets, simulators, hosted builds)
are removed, live guards stay dynamic, and every kept line is verbatim
upstream text (src/tools/flatten.py, driven by the same macro table the
build uses). config.h keeps its original upstream form — it is the
target switchboard and its chains are self-referential.
