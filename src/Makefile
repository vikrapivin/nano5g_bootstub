CROSS ?= arm-none-eabi-
HOSTCC ?= cc
PYTHON ?= python3
OUT ?= build
.DEFAULT_GOAL := all
CFLAGS = -std=gnu11 -Oz -mcpu=arm1176jzf-s -marm -mthumb-interwork \
         -ffreestanding -fno-builtin -fno-pie -fno-common -fno-stack-protector \
         -ffunction-sections -fdata-sections -Wa,--noexecstack -Wall -Wextra -Werror \
         -DIPOD_NANO5G -DTARGET_ID=134 -DMEMORYSIZE=64 \
         -Ivendor/rb/firmware/target/arm/s5l8702/ipodnano5g \
         -Ivendor/rb/firmware/target/arm/s5l8702 -Ivendor/rb/firmware/target/arm \
         -Ivendor/rb/firmware -Ivendor/rb/firmware/include -Ivendor/rb/firmware/export \
         -Ivendor/rb/firmware/kernel/include -Ivendor/rb/firmware/libc/include \
         -Ivendor/rb/bootloader -Ivendor/buildconf \
         -Ivendor/shim -Ivendor -I.
RBDIR = vendor/rb
RBCFLAGS = -std=gnu11 -W -Oz -flto=4 -nostdlib -ffreestanding -fno-common \
    -mcpu=arm1176jz-s -mno-unaligned-access -fomit-frame-pointer -mthumb \
    -ffunction-sections -fdata-sections \
    -DBOOTLOADER -DNB_FS_READONLY -DIPOD_NANO5G -DTARGET_ID=134 -DMEMORYSIZE=64 \
    -DYEAR=2026 -DMONTH=10 -DDAY=25 \
    -Ivendor/buildconf -I$(RBDIR)/firmware/target/arm/s5l8702/ipodnano5g \
    -I$(RBDIR)/firmware/target/arm/s5l8702 -I$(RBDIR)/firmware/target/arm \
    -I$(RBDIR)/firmware -I$(RBDIR)/firmware/include -I$(RBDIR)/firmware/export \
    -I$(RBDIR)/firmware/kernel/include -I$(RBDIR)/firmware/libc/include \
    -I$(RBDIR)/bootloader -include $(RBDIR)/firmware/export/config.h
$(OUT)/storage.o: $(RBDIR)/firmware/storage.c Makefile | $(OUT)
	$(CROSS)gcc $(RBCFLAGS) -MMD -MP -c $< -o $@
# The vendored rockbox read-only NAND/FTL sources keep their own warning
# profile; no -Werror for them. shim.c stays ARM (CP15 asm is ARM-only).
# Driver objects keep the shim-first include order (their config.h is the
# shim's, which defines BOOTLOADER = read-only build).
SHIMCFLAGS = -std=gnu11 -Oz -flto=4 -mcpu=arm1176jzf-s -marm -mthumb-interwork \
         -ffreestanding -fno-builtin -fno-pie -fno-common -fno-stack-protector \
         -ffunction-sections -fdata-sections -Wa,--noexecstack -Wall -Wextra \
         -Ivendor/shim -Ivendor -I.
VCFLAGS = $(filter-out -Werror,$(SHIMCFLAGS))
HOSTFLAGS = -std=c11 -g -Wall -Wextra -Werror -fno-pie -no-pie -I.
OBJECTS = $(OUT)/entry.o $(OUT)/nandboot.o $(OUT)/lcd5g.o \
          $(OUT)/bootfont.o $(OUT)/shim.o $(OUT)/nand-nano5g.o $(OUT)/ftl.o \
          $(OUT)/clocking-s5l8702.o $(OUT)/rbshim.o $(RBSTACK_OBJS)
ADDRESS_FLAGS ?=

all: $(OUT)/bootstub.bin $(OUT)/bootstub.dis $(OUT)/stub.bin

$(OUT):
	mkdir -p $@
$(OUT)/%.o: %.c Makefile | $(OUT)
	$(CROSS)gcc $(CFLAGS) -MMD -MP -c $< -o $@
# This bootstub has no USB stack; the driver code (never on-device in any
# form) builds Thumb, the boot code stays ARM.
THUMBFLAGS = $(filter-out -marm,$(VCFLAGS)) -mthumb
# The driver compiles exactly like the real bootloader build does: the
# rockbox config surface (-include config.h, -DBOOTLOADER, -DIPOD_NANO5G)
# with the flat vendor/ headers on the path. Read-only by the same
# -DBOOTLOADER the rockbox bootloader itself uses.
DRIVERFLAGS = $(filter-out -I$(RBDIR)/bootloader,$(RBCFLAGS)) -I$(OUT) -Ivendor -Wno-unused-parameter
$(OUT)/nand-nano5g.o: vendor/nand-nano5g.c $(OUT)/fmss-nano5g.h Makefile | $(OUT)
	$(CROSS)gcc $(DRIVERFLAGS) -MMD -MP -c $< -o $@
# the FMSS microcode is assembled at build time from its sources
$(OUT)/fmssasm: tools/fmssasm.c | $(OUT)
	$(HOSTCC) -O2 -o $@ $<
$(OUT)/fmss-nano5g.h: $(OUT)/fmssasm vendor/readid.s vendor/read.s
	$(OUT)/fmssasm -s $@ vendor/readid.s vendor/read.s
$(OUT)/ftl.o: vendor/ftl.c Makefile | $(OUT)
	$(CROSS)gcc $(DRIVERFLAGS) -MMD -MP -c $< -o $@
$(OUT)/clocking-s5l8702.o: $(RBDIR)/firmware/target/arm/s5l8702/clocking-s5l8702.c Makefile | $(OUT)
	$(CROSS)gcc $(RBCFLAGS) -MMD -MP -c $< -o $@
# ---- the rockbox file stack (vendor/rb, byte-identical rockbox
# sources; compiled with the bootloader build's own flag set) ----
RBSTACK = storage fat disk dir disk_cache file file_internal fileobj_mgr \
          rb_namespace pathfuncs linked_list rb-loader loader_strerror \
          crc32 unicode snprintf format common
RBSTACK_OBJS = $(addprefix $(OUT)/,$(addsuffix .o,$(RBSTACK)))
$(OUT)/fat.o: $(RBDIR)/firmware/common/fat.c Makefile | $(OUT)
	$(CROSS)gcc $(RBCFLAGS) -MMD -MP -c $< -o $@
$(OUT)/disk.o: $(RBDIR)/firmware/common/disk.c Makefile | $(OUT)
	$(CROSS)gcc $(RBCFLAGS) -MMD -MP -c $< -o $@
$(OUT)/dir.o: $(RBDIR)/firmware/common/dir.c Makefile | $(OUT)
	$(CROSS)gcc $(RBCFLAGS) -MMD -MP -c $< -o $@
$(OUT)/disk_cache.o: $(RBDIR)/firmware/common/disk_cache.c Makefile | $(OUT)
	$(CROSS)gcc $(RBCFLAGS) -MMD -MP -c $< -o $@
$(OUT)/file.o: $(RBDIR)/firmware/common/file.c Makefile | $(OUT)
	$(CROSS)gcc $(RBCFLAGS) -MMD -MP -c $< -o $@
$(OUT)/file_internal.o: $(RBDIR)/firmware/common/file_internal.c Makefile | $(OUT)
	$(CROSS)gcc $(RBCFLAGS) -MMD -MP -c $< -o $@
$(OUT)/fileobj_mgr.o: $(RBDIR)/firmware/common/fileobj_mgr.c Makefile | $(OUT)
	$(CROSS)gcc $(RBCFLAGS) -MMD -MP -c $< -o $@
$(OUT)/rb_namespace.o: $(RBDIR)/firmware/common/rb_namespace.c Makefile | $(OUT)
	$(CROSS)gcc $(RBCFLAGS) -MMD -MP -c $< -o $@
$(OUT)/pathfuncs.o: $(RBDIR)/firmware/common/pathfuncs.c Makefile | $(OUT)
	$(CROSS)gcc $(RBCFLAGS) -MMD -MP -c $< -o $@
$(OUT)/linked_list.o: $(RBDIR)/firmware/common/linked_list.c Makefile | $(OUT)
	$(CROSS)gcc $(RBCFLAGS) -MMD -MP -c $< -o $@
$(OUT)/rb-loader.o: $(RBDIR)/firmware/common/rb-loader.c Makefile | $(OUT)
	$(CROSS)gcc $(RBCFLAGS) -MMD -MP -c $< -o $@
$(OUT)/loader_strerror.o: $(RBDIR)/firmware/common/loader_strerror.c Makefile | $(OUT)
	$(CROSS)gcc $(RBCFLAGS) -MMD -MP -c $< -o $@
$(OUT)/crc32.o: $(RBDIR)/firmware/common/crc32.c Makefile | $(OUT)
	$(CROSS)gcc $(RBCFLAGS) -MMD -MP -c $< -o $@
$(OUT)/unicode.o: $(RBDIR)/firmware/common/unicode.c Makefile | $(OUT)
	$(CROSS)gcc $(RBCFLAGS) -MMD -MP -c $< -o $@
$(OUT)/snprintf.o: $(RBDIR)/bootloader/snprintf.c Makefile | $(OUT)
	$(CROSS)gcc $(RBCFLAGS) -MMD -MP -c $< -o $@
$(OUT)/format.o: $(RBDIR)/bootloader/format.c Makefile | $(OUT)
	$(CROSS)gcc $(RBCFLAGS) -MMD -MP -c $< -o $@
$(OUT)/common.o: $(RBDIR)/bootloader/common.c Makefile | $(OUT)
	$(CROSS)gcc $(RBCFLAGS) -MMD -MP -c $< -o $@
$(OUT)/rbshim.o: vendor/rbshim.c Makefile | $(OUT)
	$(CROSS)gcc $(RBCFLAGS) -MMD -MP -c $< -o $@
$(OUT)/lcd5g.o: lcd5g.c lcd5g.h Makefile | $(OUT)
	$(CROSS)gcc $(THUMBFLAGS) -MMD -MP -c $< -o $@
$(OUT)/bootfont.o: bootfont.c lcd5g.h Makefile | $(OUT)
	$(CROSS)gcc $(THUMBFLAGS) -MMD -MP -c $< -o $@
$(OUT)/shim.o: vendor/shim.c Makefile | $(OUT)
	$(CROSS)gcc $(VCFLAGS) -MMD -MP -c $< -o $@
$(OUT)/entry.o: entry.S Makefile | $(OUT)
	$(CROSS)gcc $(CFLAGS) -c $< -o $@
$(OUT)/bootstub.elf: $(OBJECTS) bootstub.ld Makefile
	$(CROSS)gcc $(filter-out -Werror,$(CFLAGS)) -nostdlib -Wl,--gc-sections -Wl,--build-id=none -flto=4 -fuse-linker-plugin \
	    $(ADDRESS_FLAGS) -T bootstub.ld -Wl,-Map,$(OUT)/bootstub.map $(OBJECTS) -lgcc -o $@
	@test -z "$$($(CROSS)nm -u $@)"
	$(CROSS)size $@
$(OUT)/bootstub.bin: $(OUT)/bootstub.elf
	$(CROSS)objcopy -O binary $< $@
$(OUT)/bootstub.dis: $(OUT)/bootstub.elf
	$(CROSS)objdump -d $< > $@

sinclude $(OBJECTS:.o=.d)

# the font-delivered entry stub (assembled from its own source)
$(OUT)/stub.bin: stub/stub.S stub/stub.ld Makefile | $(OUT)
	$(CROSS)gcc -mcpu=arm1176jzf-s -mthumb -ffreestanding -nostdlib -nostartfiles -DSILENT -c stub/stub.S -o $(OUT)/stub.o
	$(CROSS)ld -T stub/stub.ld -o $(OUT)/stub.elf $(OUT)/stub.o
	$(CROSS)objcopy -O binary $(OUT)/stub.elf $@
