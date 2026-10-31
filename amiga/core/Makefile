AMIGA_CXX ?= m68k-amigaos-g++
HOST_CXX ?= c++
BUILD ?= build
ROOT = ../..
FLAGS = -std=gnu++23 -O2 -I$(ROOT)/src -pthread
AMIGA_FLAGS = -mcrt=nix20 -m68060 -msoft-float -pthread
AMIGA_LIBS = -Wl,--wrap=snprintf -lm -lpthread -latomic
.PHONY: all native host-probes test clean
all: native
$(BUILD):
	mkdir -p $@
$(BUILD)/OpenRCT2CoreProbe: probe.cpp pthread_format.cpp $(ROOT)/src/openrct2/MemoryProfile.h | $(BUILD)
	$(AMIGA_CXX) $(FLAGS) $(AMIGA_FLAGS) -MMD -MP -MF $@.d -DOPENRCT2_SMALL_MEMORY $< pthread_format.cpp -o $@ $(AMIGA_LIBS)
$(BUILD)/OpenRCT2CoreProbe-Full: probe.cpp pthread_format.cpp $(ROOT)/src/openrct2/MemoryProfile.h | $(BUILD)
	$(AMIGA_CXX) $(FLAGS) $(AMIGA_FLAGS) -MMD -MP -MF $@.d $< pthread_format.cpp -o $@ $(AMIGA_LIBS)
native: $(BUILD)/OpenRCT2CoreProbe $(BUILD)/OpenRCT2CoreProbe-Full
$(BUILD)/probe-small: probe.cpp pthread_format.cpp $(ROOT)/src/openrct2/MemoryProfile.h | $(BUILD)
	$(HOST_CXX) $(FLAGS) -DOPENRCT2_SMALL_MEMORY $< -o $@
$(BUILD)/probe-full: probe.cpp pthread_format.cpp $(ROOT)/src/openrct2/MemoryProfile.h | $(BUILD)
	$(HOST_CXX) $(FLAGS) $< -o $@
host-probes: $(BUILD)/probe-small $(BUILD)/probe-full
$(BUILD)/test-pthread-format: tests/pthread_format.cpp pthread_format.cpp | $(BUILD)
	$(HOST_CXX) $(FLAGS) $^ -o $@
test: host-probes $(BUILD)/test-pthread-format
	$(BUILD)/test-pthread-format
	cd $(BUILD) && ./probe-small ALLOCATE && ./probe-small THREADS && ./probe-full
clean:
	rm -rf $(BUILD)
-include $(wildcard $(BUILD)/*.d)
