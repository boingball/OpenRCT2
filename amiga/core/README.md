# OpenRCT2 Amiga core investigation — milestone 0.12

The 0.1 graphics runner has passed the user's WinUAE RTG test with original G1
graphics. This milestone tackles the **actual engine**, starting with memory,
a real simulation benchmark on the host, and native C++ runtime probes.

**There is no native Amiga park simulation executable yet.** The downloadable
import executables below are diagnostics. The 0.8 viewer displays their
terrain/path export through the existing RTG backend.

## Actual native Guest pool import (0.12)

The importer now allocates real Guest storage in the actual EntityRegistry at
validated legacy slots, with its real sorted entity list, free-ID list and
spatial buckets. It populates selected fields with explicit little-endian
reads: coordinates/direction, state, outside flag, animation group, clothing,
peep ID, energy/happiness/hunger/thirst/toilet and signed 32-bit money fields
converted to the native money64 members. Missing ride/queue references are
set null, and animation objects remain unresolved. Names are not allocated.
The guest's stored legacy EntityIndex must match its array slot or import is
rejected. The portable AMAP snapshot remains unchanged.

This is still a **partial guest import, not native park simulation**. Rides,
vehicles, staff, names, histories, paths/goals, inventories and most guest
state are not reconstructed. Do not tick, install or save this isolated state
as a complete park. The viewer remains the verified 0.11 executable; this
milestone changes the import diagnostic rather than adding new graphics.

Fourteen data-only EntityRegistry method bodies are moved verbatim to
EntityRegistryData.cpp, sharing the actual allocator/list/spatial operations
with the normal engine. Entity movement and registry insertion use the same
small spatial-bucket helper. initialiseEmptyStorage is a guarded entry point
for freshly constructed empty registries only; resetAllEntities remains the
populated-state cleanup path and retains attached-name/history cleanup.

The native probe fills every one of the 10,000 slots, checks duplicate and
out-of-range allocation, rejects reinitialisation, and rebuilds the null
spatial bucket. Imported guests are checked against free/list counts, sorted
IDs and actual tile membership. A separate canonical guest fingerprint covers
the selected native Guest members and spatial indices; it does not claim
whole-guest equivalence. Independent packed wire fixtures verify exact values,
including money beyond 16-bit range and negative money. Staff/null-position
records remain excluded and invalid stored IDs are explicitly rejected.

The busy park loads 2,189 actual partial guests and leaves 7,811 slots free.
The GameState allocation remains 9,722,842 bytes on m68k; list/bucket allocations
are additional. Host/68060 comparison runs with 64 MiB and no FPU, and host
ASan/UBSan checks use LeakSanitizer disabled in this environment. Actual Amiga
or WinUAE validation of the new import path remains outstanding.

After this extraction, the rebuilt actual small-profile host engine also
passes the 1,000-tick busy-park and 2,000-tick car-ride comparisons against the
standard-profile baseline, plus oversized-map and invalid-tick rejection.
This validates the shared code on its supported host, not native simulation.

## Optional saved-guest sprite view (0.11)

The viewer can use an original RCT2 G1 file for normal walking-group guests,
with both saved clothing colours and the saved direction at all four zoom
levels. It uses a fixed neutral frame, not the saved action/animation frame.
Other groups/poses and invalid/missing sprites fall back to diagnostic markers;
`MARKERS` forces markers. No guest movement or simulation is introduced.
The importer remains at 0.10 and the AMAP v3 format is unchanged.

See [viewer instructions](../prototype/README.md). This decoder passes host
ASan/UBSan, independent bitmap/RLE references and bare 68060 tests. The native
HUNK's G1 loading, RTG viewport and cleanup match host rendering using synthetic
guest test images, including both colour remaps. The exact original G1 guest
artwork and real WinUAE/P96/CGX validation remain outstanding. No original game
art is bundled or represented by the synthetic test fixture.

## Ride catalogue and track footprints (0.10)

The partial importer now reads an isolated ride catalogue and track snapshots.
These do not populate native Ride or TrackElement instances and cannot be
simulated. Ride table fields use the actual legacy definition's offsets and
explicit byte reads: slot, legacy type, object slot, status, mode, overview
cell and station count. Invalid status/station counts are skipped and reported.
The catalogue uses legacy numeric IDs; ride names/objects are not loaded.

Track snapshots retain cell/base height, ghost/invisible flags, direction,
legacy track type and raw sequence/colour bytes, linked to a catalogue ride
(or the legacy null ride 255). A reference to an omitted/absent ride is skipped
and counted. The GameState terrain/path fingerprint remains unchanged: the
snapshots are separate and are covered by AMAP's checksum/file comparison.

AMAP v3 adds track records and the ride catalogue, while the viewer continues
to accept v1 and v2. The view outlines occupied track tiles in cyan at their
saved base heights and marks station types 1..3 in white. These are diagnostic
footprints, not reconstructed rails, supports, vehicles, shops or object
images. `+`/`-` cycle the sparse ride catalogue, highlight the selected ride in
gold and centre on its saved overview cell when that cell is in bounds. After
the last ride, selection cycles back to all rides. Tab hides/shows footprints;
Space still toggles static guest markers; Home centres the map.

The busy fixture contains 134 ride snapshots and 9,175 track footprints, with
no invalid ride entries or unresolved track references at this checkpoint.
Small TestPark contains one ride and 49 footprints. Independent wire checks
verify exact ride fields, raw track bytes, coordinates and unresolved-reference
skipping on host and 68060. Parser tests reject duplicate/unsorted catalogue
IDs, invalid status/station counts, track directions and unresolved references,
even with recomputed checksums. Selection/highlighting/toggle tests pass on
host ASan/UBSan and the bare 68060 model. Native HUNK startup/RTG/cleanup
checks pass, and busy-park viewport pixels match the host at both resolutions.
Real WinUAE/P96/CGX validation and native frame rates remain unverified.

## Saved guest position snapshots (0.9)

The importer now extracts valid guest positions into a separate view-only
snapshot vector, using explicit little-endian reads from the actual legacy
Peep field offsets. It retains slot, coordinates, direction, state, outside
flag, clothing colours and animation group. No Guest objects are constructed
in EntityRegistry, and the partial GameState remains unsuitable for ticking.

AMAP v2 appends bounded, sorted guest records; the reader still accepts v1.
The viewer draws static gold markers (cyan for outside-park guests), with
heading indicators. These are diagnostic markers, not original guest sprites
or animations; saved clothing/animation metadata is retained for future work.
Riding and picked-up states are not drawn as walkers. Space toggles markers.
Markers are drawn with their map cell, so later foreground terrain can cover
them. This is approximate painter order, not the engine's complete renderer.

The busy park exports 2,189 valid positions and skips 635 unplaced or invalid
records. The saved in-park counter is 2,775; those numbers need not match because
snapshots may include outside guests and exclude null/off-map positions. The
small TestPark has no valid guest positions, so use BusyPark for this feature.
Skipped records are reported explicitly. Bounds are diagnostic limits, not a
claim that every valid legacy save/entity state is supported.

Host and emulated 68060 snapshot bytes match. An independent wire fixture
checks exact packed coordinates, direction, clothing and ordering, and excludes
staff and a null-position guest. Parser tests reject duplicate slots, bad
ordering, count/coordinate/state/colour bounds and reserved fields, even with
recomputed checksums. Renderer tests check exact marker position, hidden
on-ride state, static output and terrain restoration when toggled off. Both
resolutions match through the mocked native RTG interface. Existing v1 map
and legacy demo tests remain covered. Host ASan/UBSan and bare 68060 checks
pass. Real WinUAE/P96 validation remains outstanding.

The old partial-state fingerprint deliberately still covers only the terrain,
paths and selected GameState metadata. AMAP's payload checksum and whole-file
SHA-256 comparisons additionally cover the new snapshot records.

## Imported terrain/path RTG viewer (0.8)

OpenRCT2ImportProbe now writes ImportedMap.amap alongside its report and PPM.
The portable export is produced from the actual partial GameState, using the
engine's slope corner heights. It contains an explicit little-endian header,
per-cell index and terrain/path records with a payload checksum. Source saves
remain read only. The host and 68060 exports match byte for byte for both real
parks and independent wire fixtures; all existing import rejection tests pass.

The standalone C RTG runner accepts `MAP=ImportedMap.amap`. It displays terrain,
corner elevations, cliffs, water, connected footpaths and queue colours, with
four zoom levels. No original G1 file is required for this diagnostic view.
See [the viewer instructions](../prototype/README.md) and
[AMAP format](../prototype/MAP_FORMAT.md). This is a view of imported data,
with simplified geometry and colours; rides, scenery, guest entities, object
assets, construction tools and simulation are still absent.

The Amiga HUNK passes relocation/startup/OS-call/cleanup tests through mocked
Amiga libraries at 640x480 and 320x240; captured RTG viewport pixels match the
host renderer. Corrupt map rejection also closes files and frees allocations.
The parser/render tests pass on the host (ASan/UBSan, LeakSanitizer disabled)
and QEMU's bare 68060 model, with identical hashes; the executable passes the
unsupported-68060-instruction scan. These are not a real P96/CGX or WinUAE test
and establish neither hardware compatibility nor frame rate.

## Partial native GameState import (0.7)

Probe 0.6's runtime/shared-pointer checks and real TestPark.sv6 decoding have
now passed the user's WinUAE tests. The next executable is OpenRCT2ImportProbe:

```text
OpenRCT2ImportProbe TestPark.sv6
```

The download includes TestPark.sv6. The tool constructs the actual small-profile
GameState_t (9,722,842 bytes on m68k), imports terrain/path elements into its
real tileElements vector, and builds the engine's TilePointerIndex. It copies
selected metadata with explicit little-endian reads: map dimensions, date,
ticks, RNG seeds, initial cash, loan, park flags, entrance fee, guest count,
rating, decrypted cash and a separate legacy scenario label. It does not
allocate the entire extended S6Data struct merely to read those fields.

The terrain/path conversion helpers are shared with the normal S6Importer.
Their data-only accessors are moved unchanged out of UI/object-heavy source
files into small translation units so they can link on Amiga. Path status
access also fixes an actual endian-dependent union: AdditionStatus aliases
the low byte of RideId, which is not its first native byte on m68k. The new
accessors preserve the little-endian semantics and the ID's upper byte.

The resulting state is deliberately partial and isolated. Ride/entity/scenery
and other elements are counted and omitted. The guest number is the saved
counter, not a set of imported guest entities. Terrain slots retain stable
legacy terrain indices; path slots use the engine's legacy-path form. No
ObjectManager or object images are loaded. This state is never installed as
the active game state and must not be ticked or passed off as a complete park.
The main Platform.h endian gate remains intact.

OpenRCT2ImportProbe.txt and a small P6 PPM overview, ImportedMap.ppm, are
written in the current writable drawer. The image uses diagnostic terrain,
water and footpath colours; it is not an OpenRCT2 viewport or ride rendering.
The input SV6 is read only. Only classic SV6 saved games are accepted in this
partial importer; SC6, modern .park and extended Classic files are rejected.
The existing 0.6 save probe remains useful for chunk-level SC6 checks.

Verified on the host and volamos 68060/no-FPU with 64 MiB RAM:

| Save | Map | Active surfaces | Paths | Partial-state fingerprint |
| --- | --- | ---: | ---: | --- |
| TestPark (small car-ride fixture) | 15 x 15 | 225 | 24 | a741afd1 |
| bpb.sv6 | 128 x 128 | 16,384 | 3,400 | 525d0b3d |

Canonical imported-field fingerprints and overview bytes match across both
architectures. An independent little-endian wire fixture checks metadata,
RNG input, queue paths and fixed PPM bytes; another checks negative cash.
Tests reject bad checksums, oversized maps, unterminated tile chains,
unsupported scenarios and truncated input, and verify saves are not modified.
Host ASan/UBSan checks pass (LeakSanitizer disabled under the sandbox).

A rebuilt full host engine also passes the prior 1,000-tick busy-park and
2,000-tick car-ride comparisons against the standard-profile baseline. This
checks that the shared conversion/refactoring changes preserve the selected
simulation fingerprints on the existing supported host. It does not establish
that native Amiga simulation works. The packaged 0.7 importer has also passed a repeat 68060/no-FPU, 64 MiB
check, including malformed files; native import still needs WinUAE validation.

Building this diagnostic requires the official nlohmann JSON forward headers,
used by the actual S6Data declaration, as well as the existing compiler:

```sh
# After installing nlohmann-json3-dev (Debian/Ubuntu):
mkdir -p amiga/core/build/headers
cp -R /usr/include/nlohmann amiga/core/build/headers/
make -C amiga/core import-probes AMIGA_CXX=/path/to/m68k-amigaos-g++
python3 amiga/core/verify_partial_import.py amiga/core/build/import-probe-host
# Optional native comparison:
python3 amiga/core/verify_partial_import.py amiga/core/build/import-probe-host \
  --native amiga/core/build/OpenRCT2ImportProbe --emulator /path/to/volamos
```

JSON_INCLUDE can instead point to another include root containing
nlohmann/json_fwd.hpp. The target uses headers only, with no JSON runtime.

## Native save decoding milestone (0.6)

The user confirmed baseline, ALLOCATE and THREADS all pass with probe 0.5
in WinUAE. This milestone adds OpenRCT2SaveProbe, linking the engine's actual
MemoryStream, SawyerChunkReader, SawyerChunkWriter and SawyerCoding sources.

```text
OpenRCT2SaveProbe
OpenRCT2SaveProbe mypark.sv6
OpenRCT2CoreProbe THREADS
```

No arguments runs fixed wire fixtures, all four compression encodings,
unaligned little-endian scalar/checksum tests and corrupt-chunk rejection.
A file argument additionally verifies the SV6/SC6 checksum, decodes the S6
header fields and every chunk, and prints a fingerprint of the decoded bytes.
Scenario-info and packed-object envelope traversal are supported. This is
chunk decoding, not semantic park validation, object loading or game-state
import. The input file is read only. OpenRCT2SaveProbe.txt is written in the
current drawer; use a writable drawer.

The file-size cap is 16 MiB and the decoded-chunk cap is the engine's 16 MiB.
Extended Classic saves are rejected. No game data is required for self-tests;
use an existing SV6/SC6 save to test real file decoding. Modern .park files
are not supported by this probe. No park simulation is running yet.

Production changes in this milestone:

- Sawyer chunk reader/skip and all chunk encoders use little-endian lengths.
- SV4/TD6 and stream-writer checksums use explicit little-endian, unaligned-safe
  loads/stores instead of uint32_t pointer casts or native-order writes.
- S6Header is extracted unchanged into its own lightweight engine header.
- IStream::WriteString uses string_view directly instead of pulling in the
  large String implementation; embedded-NUL behavior is tested and preserved.
- The upstream big-endian Platform.h rejection remains intact. Whole legacy
  structs, tile elements, Orca .park data and DataSerialiser still need audit.

A further runtime problem was isolated with a tiny shared_ptr copy/reset test:
open4's libstdc++ helper emits CAS.L for a reference count which may be aligned
only to two bytes under this ABI. It succeeds in volamos's 68020/68040 modes
and faults in 68060 mode. The diagnostic executables link amiga_atomics.cpp,
which replaces the two libstdc++ reference-count arithmetic helpers using a
tiny Exec Forbid/Permit task-context critical section, preserving the ABI.
It performs no allocation/wait inside that section. This is for single-CPU
AmigaOS task context, not interrupt callers or SMP, and does not repair every
possible atomic operation in the runtime. No engine struct packing is changed.

CoreProbe 0.6 now tests shared_ptr copy/reset by default and concurrent copies
inside THREADS. The updated THREADS test needs WinUAE/real AmigaOS validation;
volamos does not implement CreateNewProc. A previous THREADS pass on 0.5 does
not establish concurrent shared_ptr correctness in 0.6.

Verified on a little-endian host and volamos 68060 with FPU disabled/64 MiB RAM:

| Input | Chunks | Decoded-byte fingerprint |
| --- | ---: | --- |
| bpb.sv6 | 5 | 08000478 |
| small_park_car_ride_one_car.sv6 | 5 | 8e1dcc1d |
| Synthetic scenario/packed-object envelope | 7 | 5ddb9d63 |

Both architectures emit the four-encoding wire fingerprint b9cdc2a8 and reject
bad checksums, truncated files and invalid lengths. These are byte-level
compatibility results, not simulation fingerprints or 68060 performance data.
The scenario envelope is synthetic and is not a playable park.

```sh
make -C amiga/core native test AMIGA_CXX=/path/to/m68k-amigaos-g++
python3 amiga/core/verify_save_probe.py amiga/core/build/save-probe-host
# Optional local 68060 emulator comparison (volamos executable required):
python3 amiga/core/verify_save_probe.py amiga/core/build/save-probe-host \
  --native amiga/core/build/OpenRCT2SaveProbe --emulator /path/to/volamos
```

## Thread creation diagnostic update (0.5)

The user confirmed probe 0.4 passes baseline and ALLOCATE on WinUAE, but
THREADS still raises error 8000000B. Inspection found pthread_create calls
snprintf with `pthread thread #%ld` before starting each worker, reaching
the same FPU-using formatter avoided by 0.4's main output path.

Probe 0.5 links a narrow --wrap=snprintf shim for that exact thread-name
format. It preserves the formatted length, truncation and NUL semantics;
other formats still use the normal runtime. This is a probe workaround,
not a general repair of the toolchain's soft-float C library. Makefile users
must include pthread_format.cpp and the wrapper linker flag together.

The diagnostic now prints checkpoints before mutex construction, worker
creation/join and the condition-variable test. THREADS remains opt-in.

Validation: host baseline, allocation, threads and formatter boundary tests
pass. Native 68060/no-FPU baseline and small-profile allocation pass. The
native THREADS test gets past thread-name formatting and reaches
CreateNewProc, which volamos does not implement. A complete native threading
PASS therefore still requires WinUAE/real AmigaOS testing. No full park
simulation is included.

## WinUAE crash diagnostic update (0.4)

The open4-linked printf formatter unconditionally saves FPU registers even
for integer-only output. Compiling the probe with `-msoft-float` does not
remove those instructions from the linked runtime. This can explain a
line-F failure before any output on a machine without usable FPU support;
it is not yet a confirmed diagnosis of the user's WinUAE exception.

Probe 0.4 uses a small integer/string formatter instead. Each progress line
is flushed to both Shell and report. Run the small probe without arguments
first, then ALLOCATE. THREADS is now opt-in so baseline/memory tests can be
checked independently. A baseline PASS does not qualify the threading runtime.
The full-profile probe is optional and needs substantially more memory.

Validation: both profiles build with open4; the host tests pass with THREADS
and ALLOCATE. Under volamos 68060 with FPU disabled, baseline and allocation
pass for small (64 MiB emulated RAM) and full (128 MiB) profiles. The original
download fails in that environment. THREADS still stops in volamos and is
unqualified on AmigaOS; no WinUAE success is claimed for this update.

## What changed

- Optional `OPENRCT2_SMALL_MEMORY` CMake profile restores original RCT2 limits:
  10,000 entity slots, 255 rides, a 256 x 256 technical map (254 x 254 usable),
  and 196,608 tile elements. It is **off by default**.
- The profile bounds miscellaneous effects at 300 slots, also used as their
  allocation reserve. Retaining the modern 6,400-slot reserve in a 10,000-slot
  pool would suppress effects much too early in a busy park.
- Fixed entity arrays, per-tile spatial vector arrays, ride storage and map
  lookup storage consequently become smaller. IDs keep their original widths.
- Oversized `.park` map dimensions, tile-element counts, ride IDs and entity
  IDs fail explicitly in the small profile. Extended RCT Classic saves fail
  explicitly. Maps with invalid dimensions in legacy S6 files also fail.
  No save is deliberately truncated to fit.
- `openrct2-core-bench` links the actual `libopenrct2`, loads a real saved park,
  and calls `gameStateUpdateLogic()`. Guest, vehicle, ride, weather, research
  and park updates are the engine's own code, not the graphics runner's
  scripted crowd.
- `OpenRCT2CoreProbe` is a native Amiga HUNK program compiled against the
  actual core headers, with C++23, libnix, libpthread and soft-float 68060 code generation.
  It tests vector/sort, the engine RNG, byte swapping, exceptions, real worker
  threads, mutexes, condition variables, atomics, thread-local storage,
  call_once and optional memory allocation. It does not construct a working game or load a park.

The standard game profile retains its existing limits. Original data-file
layouts and their legacy constants are not resized.
The experimental profile requires `DISABLE_NETWORK=ON`: its different
capacities are not advertised as compatible with normal multiplayer peers.

## Native 68k memory layout

Measured by compiling the actual headers with OpenAmigaGCC 16.2.0-open4:

| Structure | Standard profile | Small-memory profile |
| --- | ---: | ---: |
| `GameState_t` | 60,545,908 bytes | 9,722,842 bytes |
| `EntityRegistry` (included in GameState) | 45,643,648 bytes | 5,916,612 bytes |
| One `Ride` | 14,894 bytes | 14,894 bytes |

That is about **58 MiB down to 9.3 MiB** for GameState alone. The reduction
does not imply the whole game fits in 64 MB: code, objects, graphics, tile
elements, histories, import buffers, OS allocations and RTG memory are extra.
The 64-bit host has different layouts; do not substitute its sizeof values
for these native numbers.

## Run the native probes

Unpack onto an Amiga hard disk. From a Shell in the drawer:

```text
Protect OpenRCT2CoreProbe +e
OpenRCT2CoreProbe
OpenRCT2CoreProbe ALLOCATE
OpenRCT2CoreProbe THREADS
```

It writes `OpenRCT2CoreProbe.txt` in the current drawer, and prints to the
Shell. `ALLOCATE` reserves and touches a block **equal in size to GameState**;
it does not initialise GameState or start the engine. Omitting ALLOCATE skips
that reservation. The optional `OpenRCT2CoreProbe-Full` reports the normal
engine's layout; only run its ALLOCATE mode if sufficient Fast RAM is free.

No original game data is needed. The threading test depends on the
OpenAmigaGCC pthread runtime; see the toolchain documentation for its setup. Use a 68060 with Fast RAM. Stack setup requests 256 KiB.

## Remaining native blockers

These are verified build failures, rather than inferred performance problems:

1. `src/openrct2/platform/Platform.h` contains a static assertion rejecting
   big-endian targets, stating the engine is known to be broken on them.
   A direct native compilation of `GameState.cpp` hits it. This assertion
   remains intact. G1's explicit little-endian decoder in the 0.1 runner does
   not solve raw legacy save structs, map elements and other core formats.
2. Threading is no longer a compiler dead-end: OpenAmigaGCC 16.2.0-open4
   supplies libpthread and a C++23 POSIX-thread runtime. The new probe now
   exercises thread creation/join, mutexes, condition variables, atomics,
   thread-local storage and call_once. The actual test still needs to pass on
   WinUAE/real AmigaOS before the engine can rely on it.
3. Full engine platform integration and remaining library dependencies,
   including archive/compression and Unicode support, still need native work.

Do not bypass the endianness assertion and call the resulting executable a
working port. The next native milestone is a tested file/serialization and
threading adaptation, then the genuine headless simulation on Amiga.

## Build native probes

Use OpenAmigaGCC 16.2.0-open4 (with libnix, libpthread and libstdc++), not a
bare freestanding compiler without a C++ standard library. The release is
[available here](https://github.com/DalsinAI/openamigagcc/releases/tag/v16.2.0-open4):

```sh
cd amiga/core
make native AMIGA_CXX=/path/to/toolchain/bin/m68k-amigaos-g++
make test
```

The probe is built with C++23 and links `-lpthread -latomic`. It deliberately
uses no AmigaOS NDK headers, so this diagnostic can be built with the published
open4 toolchain alone. The AmigaOS pthreads runtime must be present when running
it. The small-profile ALLOCATE test checks the same 9,722,842-byte GameState
reservation; the new thread tests verify real OS thread creation and joins.
This is a runtime qualification test, not a park simulation or timing result.

## Build and run the real core benchmark

On Debian/Ubuntu, install CMake, Ninja, a C++20 compiler and development
packages for libzip, zlib, zstd, libpng, ICU and nlohmann-json.

```sh
cmake -S . -B build/core-small -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DDISABLE_IPO=ON \
  -DOPENRCT2_BUILD_CORE_BENCH=ON -DOPENRCT2_SMALL_MEMORY=ON \
  -DDISABLE_GUI=ON -DENABLE_SCRIPTING=OFF \
  -DDISABLE_NETWORK=ON -DDISABLE_HTTP=ON -DDISABLE_TTF=ON \
  -DDISABLE_OPENGL=ON -DDISABLE_DISCORD_RPC=ON \
  -DDISABLE_VERSION_CHECKER=ON -DDISABLE_FLAC=ON -DDISABLE_VORBIS=ON \
  -DDOWNLOAD_OBJECTS=OFF -DDOWNLOAD_TITLE_SEQUENCES=OFF \
  -DDOWNLOAD_OPENSFX=OFF -DDOWNLOAD_OPENMUSIC=OFF
cmake --build build/core-small --target openrct2-core-bench -j4
```

For the GCC 13 third-party picosha2 `-Wstringop-overflow` diagnostic encountered
in this environment, the local validation build also used
`-DCMAKE_CXX_FLAGS=-Wno-error=stringop-overflow`. That warning stays visible;
the repository's warning policy has not been changed.

Prepare a data directory with `language/` from this repository's `data/` and
`object/` containing the official OpenRCT2 objects (the unbuilt JSON sources
work in this no-graphics benchmark). Do not run against a normal user profile:
pass a dedicated disposable directory for the benchmark's config/cache.

```sh
build/core-small/amiga/core/openrct2-core-bench \
  test/tests/testdata/parks/bpb.sv6 1000 \
  /path/to/headless-data /path/to/disposable-benchmark-user 40
```

Arguments are park, measured ticks, data directory, private user directory and
optional warmup ticks. The benchmark does not save/overwrite the input park.
It fails instead of producing a result if park loading fails or the requested
number of simulation ticks does not advance. It reports mean/median/p95/max
tick time, process peak RSS on Linux, and a selected-state fingerprint.

The fingerprint uses canonical numeric fields (date/tick/RNG/cash/counts,
active entity positions/IDs/orientation and ride type/status). It excludes
pointers, padding and unused array capacity. It is **not** a complete game
checksum. The normal network checksum is stubbed to zero when networking is
disabled, so using that would provide no evidence at all.

The included `bpb.sv6` fixture has a 128 x 128 map, around 2,745 guests and
134 rides at this checkpoint. A normal engine tick is 1/40 second; headless
benchmarks run uncapped. Desktop results do not predict 68060 performance.

## Verified profile checks

`verify_profiles.py` runs the normal and constrained engines against both
`bpb.sv6` (1,000 measured ticks) and `small_park_car_ride_one_car.sv6` (2,000),
with 40 warmup ticks each. Guest/ride counts, ticks and selected-state
fingerprints match between profiles. The final fingerprints are `d3ad677c`
and `ec449c62` respectively for this source/fixture checkpoint.

It also feeds a checksummed minimal `.park` envelope with a 257-tile map into
the actual importer, verifies explicit rejection, and checks invalid tick
arguments. It does not claim exhaustive park/save compatibility.

```sh
python3 amiga/core/verify_profiles.py \
  build/core-full/amiga/core/openrct2-core-bench \
  build/core-small/amiga/core/openrct2-core-bench \
  /path/to/headless-data /path/to/disposable-comparison-work
```

The host's peak process RSS in the busy-park check fell from roughly 141 MiB
to 41 MiB. That is a 64-bit Linux process measurement, not an Amiga RAM
requirement or a promise that the entire native game will fit in 64 MB.
