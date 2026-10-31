# Standalone graphics prototype. Does not build or replace OpenRCT2's C++ core.
CC ?= cc
CC68K ?= m68k-linux-gnu-gcc
PYTHON ?= python3
QEMU ?= qemu-m68k
OBJDUMP ?= m68k-linux-gnu-objdump
BUILD ?= build
WARN = -Wall -Wextra -Werror -Wno-misleading-indentation
CORE = src/g1.c src/runner.c src/park_map.c src/park_render.c
AMIGA_FLAGS = -m68060 -msoft-float -O2 $(WARN) -ffreestanding -fno-builtin -fno-pic -fno-pie -fno-common -fomit-frame-pointer -nostdlib -static -Isrc -Wl,--emit-relocs -Wl,--no-warn-rwx-segments -Wl,--build-id=none -Wl,-T,amiga.ld
.PHONY: all amiga test test-asan test-68k check-060 smoke-hunk host clean
all: amiga
$(BUILD):
	mkdir -p $@
$(BUILD)/OpenRCT2Runner.elf: $(CORE) src/platform_amiga.c src/start.S src/runner.h src/park_map.h src/guest_palette.h src/asm.h amiga.ld | $(BUILD)
	$(CC68K) $(AMIGA_FLAGS) -o $@ src/start.S $(CORE) src/platform_amiga.c -lgcc
$(BUILD)/OpenRCT2Runner: $(BUILD)/OpenRCT2Runner.elf tools/elf2hunk.py
	$(PYTHON) tools/elf2hunk.py $< $@
amiga: $(BUILD)/OpenRCT2Runner
$(BUILD)/test_runner: tests/test_runner.c $(CORE) src/runner.h src/park_map.h src/guest_palette.h | $(BUILD)
	$(CC) -O2 $(WARN) -Isrc -o $@ tests/test_runner.c $(CORE)
test: $(BUILD)/test_runner
	$<
$(BUILD)/test_asan: tests/test_runner.c $(CORE) src/runner.h src/park_map.h src/guest_palette.h | $(BUILD)
	$(CC) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-omit-frame-pointer -o $@ tests/test_runner.c $(CORE)
test-asan: $(BUILD)/test_asan
	$<
$(BUILD)/test_68k: tests/test_runner.c $(CORE) src/runner.h src/park_map.h src/guest_palette.h | $(BUILD)
	$(CC68K) -O2 -static -m68060 -msoft-float $(WARN) -Isrc -o $@ tests/test_runner.c $(CORE)
test-68k: $(BUILD)/test_68k
	$(QEMU) -cpu m68060 $<
check-060: $(BUILD)/OpenRCT2Runner.elf
	OBJDUMP=$(OBJDUMP) $(PYTHON) tools/check_060.py $<
smoke-hunk: $(BUILD)/OpenRCT2Runner
	$(PYTHON) tests/smoke_hunk.py $<
$(BUILD)/runner_host: src/platform_host.c $(CORE) src/runner.h src/park_map.h src/guest_palette.h | $(BUILD)
	$(CC) -O2 $(WARN) -Isrc -o $@ src/platform_host.c $(CORE)
host: $(BUILD)/runner_host
clean:
	rm -rf $(BUILD)
