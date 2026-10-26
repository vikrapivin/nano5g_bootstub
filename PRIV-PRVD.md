# The PRIV/PRVD gate — how the untethered boot is allowed

Why a cold, battery-powered boot with a PATCHED resource image does not
fail signature checks: the device's own FirmwareStatus flag, bit1,
tells the boot loader to skip rsrc validation. Every claim below is
either an on-device observation (marked "device") or a fact read out of
the firmware images (marked with an address).

## 1. Where the flag lives

The PrivateData store = the private partition of the NAND disk image
(disk LBA 1 — 4096-byte LBAs, so disk byte offset 0x1000; MBR entry 2:
type 0x3e, start LBA 1, len 4). The store is the FIRST 0x200 bytes of
that partition. At every boot `Bds_CheckBoot` (bl_Bds RVA 0x10f8 in the
boot loader image) locates the PrivateData protocol (GUID
dccd135d-44f0-406a-85c3-a220ec4eb82a) and calls its method +8 =
`PrivateData_GetFirmwareStatus` (bl_PrivateData RVA 0x2d0), which
answers from this block. Bds tests bit0/bit1 of the returned u32.

### 1.1 The store block, byte by byte

Two on-NAND header shapes are accepted by the reader
(`PrivateData_ReadStore`, bl_PrivateData RVA 0x420):

    offset   plain (factory shipped)         PRVD (what enables the untether)
    ------   ---------------------------     ----------------------------------------
    +0x00    u32  1            (magic)      u32  0x44565250 = "PRVD" (LE)
    +0x04    u32  0                         u32  2          (version: PRVDv2)
    +0x08    u32  4                         u32  0
    +0x0c    u32  4                         u32  4
    +0x10    .. zeros ..                    .. zeros ..
    +0x40    .. zeros ..                    .. zeros ..
    +0x44    .. zeros ..                    .. zeros ..
    +0x80    .. zeros ..                    16 B  integrity tag
    +0x90    .. zeros ..                    .. zeros ..
    +0x100   .. zeros ..                    .. zeros ..    (block is 0x200 bytes)

    In the PRVD shape only:

    +0x40    u32  FirmwareStatus            <- the value Bds reads; bit1
             (= 2) makes Bds skip rsrc signature validation. The shipped
             untether writes 0x00000002 (bit1 only; bit0 was clear in
             every observed on-device state).
    +0x80    16 B  integrity tag            <- authenticates the block
             (construction in 1.2 below)

    Everything not listed is zero in both shapes; the whole block is
    written and read as one 0x200-byte unit at disk LBA 1.

For the plain shape, `PrivateData_ReadStore` returns a HARDCODED 0:
no field of the plain block is a status. Bit1 therefore cannot be set
by a plain NAND write — the PRVD shape (with a valid tag) is the only
way to hand Bds a nonzero FirmwareStatus.

For the PRVD shape, the store returns `u32@+0x40` **iff the tag
verifies**; a PRVD block whose tag does not verify is rejected (the
store falls back to the failed/plain path, not to attacker data).

### 1.2 The integrity tag at +0x80 — exact construction

    tag = AES-128-Encrypt_K_uid( SHA1( block[0 .. 0x80] )[0:16] )

step by step:

 1. SHA1 over the first 0x80 bytes of the block — i.e. the header words
    at +0x00/+0x04/+0x08/+0x0c, the zeros +0x10..+0x40, the
    FirmwareStatus u32 at +0x40, and the zeros +0x44..+0x80. Take the
    FIRST 16 BYTES of the 20-byte digest.
 2. AES-128 ENCRYPT those 16 bytes, ECB (zero IV), with the key the
    S5L87xx hardware AES engine addresses as KEY SELECTOR 2 = the
    per-device UID key class — a 128-bit key baked into the chip,
    readable by no software path, unique per unit (selector 1 is the GID
    class the bootROM
    uses for its IMG1 header tags; the PRVD tag is the same
    construction as those bootROM tags but under the UID key).
 3. Store the 16 ciphertext bytes at +0x80.

Verification by the device is the symmetric inverse: it recomputes
SHA1(block[0..0x80])[0:16], DECRYPTS the stored tag with selector-2
AES, and compares. (The hardware engine reference: the boot loader's
own AES routine at fw_disk 0x080304b8, MMIO-verified against the
hardware.)

### 1.3 Why this is a gate, not a byte flip

 * The plain shape has NO status field: whatever bytes a plain-shaped
   private partition contains, `PrivateData_ReadStore` returns a
   hardcoded 0. No plain NAND write — to any offset — can make Bds
   see bit1.
 * The PRVD shape without a valid tag is rejected the same way: the
   store never hands Bds the attacker's +0x40 value, it just doesn't
   produce a nonzero status at all. PRVD shape AND valid tag is the
   only door to a nonzero FirmwareStatus.
 * The tag key is the chip-unique UID key (selector 2), readable by no
   software path. No host can compute a valid tag for any block —
   offline forgery is impossible by construction. Because the AES is
   symmetric, the only party that can produce a tag for a given device
   is the device itself.
 * Consequence for an exploit: a bare NAND write primitive cannot open
   this gate. It takes code execution on the unit (to compute the tag
   with the hardware engine) plus the write (the forged 0x200-byte
   block to disk LBA 1). The write primitive delivers the block; the
   on-device execution vouches for it.
 * Consequence for persistence: the block is ordinary NAND contents
   and the OS itself maintains status 2 (see 2), so the gate survives
   reboots and normal OS operation — which is what makes the untether
   untethered: cold boot, battery only, no USB, no buttons.

## 2. How it is set on the device

* The OS sets it itself: OSOS `App_Init:0x081ac464` ->
  `Rsrc_Mount:0x080b123c` -> `PrivSlot0_SetState(2):0x08191ae4` —
  Apple's own resource mount writes PRVDv2 with status 2 (bit1) and
  caches it. Bit1 is not an attacker-invented state; it is a
  legitimate Apple state the OS maintains, which is why the gate
  survives normal OS boots.
* Device evidence (read-only, on hardware): the device's private block
  reads PRVDv2, status 0x00000002 at +0x40 — bit0 clear, bit1 set —
  tag 8174493e3f2413b261438bc305b29265 at +0x80.
* Setting it from a write primitive: build the 0x200-byte block as in
  1.1/1.2 with the chosen FirmwareStatus, have the DEVICE compute the
  tag with its selector-2 AES (any code execution path on the unit
  works; the tag must be produced on the unit it will be verified on),
  write the block to disk LBA 1, read it back and byte-compare. A
  deterministic forge run twice yields identical blocks.

## 3. What bit1 skips

In the firmware partition only the IMAGE BODIES are RSA-validated; the
directory and the private partition never are. The rsrc image (IMG1
format 4, plaintext FAT16 with Resources/Fonts inside) is one of those
bodies — the untether font swap (the star charstring at MSE 0x93FA00)
modifies it. With bit1 set, `Bds_CheckBoot` short-circuits: the rsrc
body is never signature-checked, so the patched font loads.

## 4. The resulting untethered chain (what this bootstub is)

Every cold boot — battery, no USB, no buttons required:

  Bds_CheckBoot -> bit1 -> rsrc loads unvalidated
  -> FreeType renders the star charstring
  -> the Type2 chunk encoding writes the stub to 0x22050000 (the
     shipped stub branches straight to the handoff — no green screen)
     and the payload to 0x22060000
  -> hijacked LR jumps to the stub -> handoff to nb_boot_main
  -> watchdog off -> guarded timer -> storage_init (read-only NAND+FTL)
  -> filesystem_init -> disk_mount_all
  -> load_firmware("/.rockbox/rockbox.ipod")  [rockbox's own file stack,
     vendored under src/vendor/rb]
  -> cache flush + ARM jump to Rockbox at 0x08000000.

## 5. Recovery / audit

Reading the private partition's first 0x200 bytes shows the header
format (plain `{1,0,4,4}` vs "PRVD"), the raw status u32 at +0x40 and
the stored tag at +0x80. Restoring the plain `{1,0,4,4}` header clears
the gate (boot then demands a valid rsrc again), and restoring the
factory rsrc font region undoes the exploit itself.
