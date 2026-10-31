AMIGA_CXX ?= m68k-amigaos-g++
HOST_CXX ?= c++
BUILD ?= build
ROOT = ../..
FLAGS = -std=gnu++23 -O2 -I$(ROOT)/src -pthread
AMIGA_FLAGS = -mcrt=nix20 -m68060 -msoft-float -pthread
AMIGA_LIBS = -Wl,--wrap=snprintf -lm -lpthread -latomic

SAVE_SOURCES = save_probe.cpp save_probe_support.cpp amiga_atomics.cpp \
 $(ROOT)/src/openrct2/core/MemoryStream.cpp $(ROOT)/src/openrct2/core/IStream.cpp \
 $(ROOT)/src/openrct2/core/StreamBuffer.cpp \
 $(ROOT)/src/openrct2/sawyer_coding/SawyerChunk.cpp \
 $(ROOT)/src/openrct2/sawyer_coding/SawyerChunkReader.cpp \
 $(ROOT)/src/openrct2/sawyer_coding/SawyerChunkWriter.cpp \
 $(ROOT)/src/openrct2/sawyer_coding/SawyerCoding.cpp
SAVE_OBJECTS = $(addprefix $(BUILD)/native/,$(notdir $(SAVE_SOURCES:.cpp=.o)))
RUNTIME_OBJECTS = $(BUILD)/native/pthread_format.o $(BUILD)/native/amiga_atomics.o
vpath %.cpp . $(sort $(dir $(SAVE_SOURCES)))

.PHONY: all native host-probes test clean save-probes
all: native
$(BUILD) $(BUILD)/native:
	mkdir -p $@
$(BUILD)/native/%.o: %.cpp | $(BUILD)/native
	$(AMIGA_CXX) $(FLAGS) $(AMIGA_FLAGS) -MMD -MP -c $< -o $@
$(BUILD)/native/probe-small.o: probe.cpp | $(BUILD)/native
	$(AMIGA_CXX) $(FLAGS) $(AMIGA_FLAGS) -MMD -MP -DOPENRCT2_SMALL_MEMORY -c $< -o $@
$(BUILD)/native/probe-full.o: probe.cpp | $(BUILD)/native
	$(AMIGA_CXX) $(FLAGS) $(AMIGA_FLAGS) -MMD -MP -c $< -o $@
$(BUILD)/OpenRCT2CoreProbe: $(BUILD)/native/probe-small.o $(RUNTIME_OBJECTS)
	$(AMIGA_CXX) $(AMIGA_FLAGS) $^ -o $@ $(AMIGA_LIBS)
$(BUILD)/OpenRCT2CoreProbe-Full: $(BUILD)/native/probe-full.o $(RUNTIME_OBJECTS)
	$(AMIGA_CXX) $(AMIGA_FLAGS) $^ -o $@ $(AMIGA_LIBS)
$(BUILD)/OpenRCT2SaveProbe: $(SAVE_OBJECTS)
	$(AMIGA_CXX) $(AMIGA_FLAGS) $^ -o $@ -lm -lpthread -latomic
native: $(BUILD)/OpenRCT2CoreProbe $(BUILD)/OpenRCT2CoreProbe-Full $(BUILD)/OpenRCT2SaveProbe

$(BUILD)/probe-small: probe.cpp diagnostic_output.h $(ROOT)/src/openrct2/MemoryProfile.h | $(BUILD)
	$(HOST_CXX) $(FLAGS) -DOPENRCT2_SMALL_MEMORY $< -o $@
$(BUILD)/probe-full: probe.cpp diagnostic_output.h $(ROOT)/src/openrct2/MemoryProfile.h | $(BUILD)
	$(HOST_CXX) $(FLAGS) $< -o $@
$(BUILD)/test-pthread-format: tests/pthread_format.cpp pthread_format.cpp | $(BUILD)
	$(HOST_CXX) $(FLAGS) $^ -o $@
$(BUILD)/save-probe-host: $(SAVE_SOURCES) diagnostic_output.h | $(BUILD)
	$(HOST_CXX) $(FLAGS) $(SAVE_SOURCES) -o $@
host-probes: $(BUILD)/probe-small $(BUILD)/probe-full
save-probes: $(BUILD)/save-probe-host $(BUILD)/OpenRCT2SaveProbe
test: host-probes $(BUILD)/test-pthread-format $(BUILD)/save-probe-host
	$(BUILD)/test-pthread-format
	cd $(BUILD) && ./probe-small ALLOCATE && ./probe-small THREADS && ./probe-full && ./save-probe-host
clean:
	rm -rf $(BUILD)
-include $(wildcard $(BUILD)/native/*.d)

# JSON forward headers are needed by the actual legacy S6Data declaration.
# Point JSON_INCLUDE at an include root containing nlohmann/json_fwd.hpp.
JSON_INCLUDE ?= $(BUILD)/headers
IMPORT_SOURCES = $(filter-out save_probe.cpp,$(SAVE_SOURCES)) import_probe.cpp s6_partial_import.cpp s6_map_export.cpp \
 $(ROOT)/src/openrct2/rct12/TileElementData.cpp \
 $(ROOT)/src/openrct2/rct2/S6ElementImport.cpp \
 $(ROOT)/src/openrct2/drawing/ColourData.cpp \
 $(ROOT)/src/openrct2/world/tile_element/Slope.cpp \
 $(ROOT)/src/openrct2/world/tile_element/ElementData.cpp
IMPORT_OBJECTS = $(addprefix $(BUILD)/import-native/,$(notdir $(IMPORT_SOURCES:.cpp=.o)))
vpath %.cpp $(ROOT)/src/openrct2/drawing $(ROOT)/src/openrct2/rct12 $(ROOT)/src/openrct2/rct2 $(ROOT)/src/openrct2/world/tile_element
$(BUILD)/import-native:
	mkdir -p $@
$(BUILD)/import-native/%.o: %.cpp | $(BUILD)/import-native
	$(AMIGA_CXX) $(FLAGS) $(AMIGA_FLAGS) -DOPENRCT2_SMALL_MEMORY -I$(JSON_INCLUDE) -MMD -MP -c $< -o $@
$(BUILD)/OpenRCT2ImportProbe: $(IMPORT_OBJECTS)
	$(AMIGA_CXX) $(AMIGA_FLAGS) $^ -o $@ -lm -lpthread -latomic
$(BUILD)/import-probe-host: $(IMPORT_SOURCES) diagnostic_output.h s6_partial_import.h | $(BUILD)
	$(HOST_CXX) $(FLAGS) -DOPENRCT2_SMALL_MEMORY -I$(JSON_INCLUDE) $(IMPORT_SOURCES) -o $@
.PHONY: import-probes
import-probes: $(BUILD)/import-probe-host $(BUILD)/OpenRCT2ImportProbe
-include $(wildcard $(BUILD)/import-native/*.d)
