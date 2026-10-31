AMIGA_CXX ?= m68k-amigaos-g++
HOST_CXX ?= c++
BUILD ?= build
ROOT = ../..
FLAGS = -std=gnu++20 -O2 -fno-char8_t -I$(ROOT)/src
AMIGA_FLAGS = -mcrt=nix20 -m68060 -msoft-float -pthread
.PHONY: all native host-probes test clean
all: native
$(BUILD):
	mkdir -p $@
$(BUILD)/OpenRCT2CoreProbe: probe.cpp $(ROOT)/src/openrct2/MemoryProfile.h | $(BUILD)
	$(AMIGA_CXX) $(FLAGS) $(AMIGA_FLAGS) -MMD -MP -MF $@.d -DOPENRCT2_SMALL_MEMORY $< -o $@ -lm
$(BUILD)/OpenRCT2CoreProbe-Full: probe.cpp $(ROOT)/src/openrct2/MemoryProfile.h | $(BUILD)
	$(AMIGA_CXX) $(FLAGS) $(AMIGA_FLAGS) -MMD -MP -MF $@.d $< -o $@ -lm
native: $(BUILD)/OpenRCT2CoreProbe $(BUILD)/OpenRCT2CoreProbe-Full
$(BUILD)/probe-small: probe.cpp $(ROOT)/src/openrct2/MemoryProfile.h | $(BUILD)
	$(HOST_CXX) $(FLAGS) -DOPENRCT2_SMALL_MEMORY $< -o $@
$(BUILD)/probe-full: probe.cpp $(ROOT)/src/openrct2/MemoryProfile.h | $(BUILD)
	$(HOST_CXX) $(FLAGS) $< -o $@
host-probes: $(BUILD)/probe-small $(BUILD)/probe-full
test: host-probes
	cd $(BUILD) && ./probe-small ALLOCATE && ./probe-full
clean:
	rm -rf $(BUILD)
-include $(wildcard $(BUILD)/*.d)
