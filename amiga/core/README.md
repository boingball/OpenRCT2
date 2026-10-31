# OpenRCT2 Amiga core investigation — milestone 0.3

The 0.1 graphics runner has passed the user's WinUAE RTG test with original G1
graphics. This milestone tackles the **actual engine**, starting with memory,
a real simulation benchmark on the host, and native C++ runtime probes.

**There is no native Amiga park simulation executable yet.** The downloadable
0.3 executables below are diagnostics. The 0.1 graphics runner remains the
working RTG test.

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
OpenRCT2CoreProbe ALLOCATE
```

It writes `OpenRCT2CoreProbe.txt` in the current drawer, and prints to the
Shell. `ALLOCATE` reserves and touches a block **equal in size to GameState**;
it does not initialise GameState or start the engine. Omitting ALLOCATE skips
that reservation. The optional `OpenRCT2CoreProbe-Full` reports the normal
engine's layout; only run its ALLOCATE mode if sufficient Fast RAM is free.

No original game data or other runtime library needs installing for these
libnix-linked probes. Use a 68060 with Fast RAM. Stack setup requests 256 KiB.

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
