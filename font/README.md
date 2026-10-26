# The exploit font (packaging step)

The payload is delivered to the device inside the resource image's
CinecavXMono font file: the star charstring in glyph 'space' encodes
the stub (808 B -> 0x22050000) and the payload (-> 0x22060000), and
hijacks the CFF decoder's return address into the stub.

`make_exploit_font.py` builds this font **from your own firmware image**
(no Apple content ships in this tree):

    1. download the iPod nano 5G restore IPSW "iPod_1.0.2_34A20020.ipsw",
       unzip it, take "Firmware.MSE"
       (136,785,920 B, sha1 910222822725573d8c0b7c7e399aedef17c118b8)
    2. build the payload and the stub:
         cd ../src && make       -> build/bootstub.bin, build/stub.bin
    3. python3 make_exploit_font.py --mse Firmware.MSE \
         --stub ../src/build/stub.bin \
         --payload ../src/build/bootstub.bin \
         --payload-addr 0x22060000 \
         --out exploit_font.otf \
         [--patch-mse Firmware.patched.MSE]

Requirements: python3 + fontTools:

    pip install -r requirements.txt

(fonttools==4.67.0, the version the builds were verified with; other
dependencies: none — the scripts otherwise use only the Python standard
library.)

The build is deterministic: the container is derived inside the tool
from the stock fonts of your MSE, the only save-time variable (the
head 'modified' timestamp) is fixed, and the font comes out the same
from the same inputs. The printed size is checked against the
165,416-byte slot (see ../SIZE-LIMITS.md) — an oversized payload is
rejected before any write to the device.

## Write coordinates

The font goes into the firmware image at

    offset 0x93FA00, length 165,416 bytes (the built font padded with
    zeros to the region end; the slot is the rsrc "Helvetica" region)

i.e. the font padded to 165,416 bytes at the fw partition + 0x93FA00 is
the only edit needed for the untethered boot. Nothing else in the image
changes. Device raw LBA = 64 + (0x93FA00 >> 12) = 2431, intra-sector
offset 0xA00, 42 sectors; `../../emCORE/UPLOAD-BOOTSTUB.md` walks the
whole write with this distribution's emCORE.
