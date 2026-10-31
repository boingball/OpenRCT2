# Classic 68060 integration

This branch combines the constrained-memory work from `boingball/OpenRCT2`
with Thomas Lübker's native AmigaOS port, pinned to
`thomas-luebker/openrct2-amiga` branch `amiga`, commit
`6effbafba281146a61f6cbf3fe08da4d7bd6865c` (7 October 2026).
The common ancestor is `3418b4f8bf2a74394c1d52e01a603402ee2dacaa`.
Thomas's platform, rendering, input, audio, allocation and endian work remains
credited to him under the project's GPLv3 licence. See `AMIGA_PORT.md`,
`contributors.md`, `README.upstream.md` and `licence.txt`.

This is an integration candidate, not a validated 64 MB release. Earlier
0.13 diagnostic binaries remain a separate checkpoint; their WinUAE passes
do not establish that this full application is stable or fits 64 MB.

## Memory profile

Configure with `-DOPENRCT2_SMALL_MEMORY=ON -DDISABLE_NETWORK=ON`:

| Capacity | Constrained profile | Normal profile |
| --- | ---: | ---: |
| Map side | 256 | 1001 |
| Entity IDs | 10,000 | 65,535 |
| Rides | 255 | 1000 |
| Tile elements | 196,608 | 16,776,704 |

Keep Thomas's native chunk allocation: 512 entity slots per chunk, 16 rides
per chunk, and spatial buckets sized to the loaded map. Preserve stable
entity/ride addresses and the allocator's return of large blocks to Exec.

Station arrays deliberately remain 255 entries. Thomas documented heap
corruption exposed by lowering that cap; reducing it before isolating that
write would be unsafe. Graphics, object data and runtime allocations still
need measurement. These caps do not guarantee the whole game fits 64 MB.
Oversized modern parks must be rejected by the existing profile guards;
networking is excluded for this profile.

## Threading

`OPENRCT2_AMIGA_THREADS=OFF` is the default. Jobs execute inline as in
Thomas's tested port. Rendering remains single-threaded.

`-DOPENRCT2_AMIGA_THREADS=ON` enables at most two job workers. The link-time
`pthread_create` adapter supplies 256 KiB when the caller supplies no
attributes, and preserves explicit attributes. A 68060 is single-core;
this is runtime compatibility work, not a promised speed improvement.

`OPENRCT2_AMIGA_OPEN4_RUNTIME=ON` selects our previously tested open4
`snprintf` adapter and task-context libstdc++ shared-pointer atomicity
helpers. The latter avoid misaligned CAS.L reference-count accesses on
68060. Set this OFF only when validating another toolchain/runtime.

Host tests compile the real `JobPool.cpp` under its Amiga branches. They
check work/completion counts, callback thread, reuse and repeated shutdown.
They do not substitute for the full game's threaded AmigaOS tests.

## Build

Use OpenAmigaGCC 16.2.0 open4, the AHI developer C headers, and static
zlib/zstd/libpng/libzip plus nlohmann JSON headers in the dependency prefix.

```sh
AMIGA_CPU_FLAGS='-m68060 -msoft-float' \
  sh distribution/amiga/configure.sh build-060 /path/to/amiga-deps \
  /path/to/openamigagcc -DOPENRCT2_SMALL_MEMORY=ON
cmake --build build-060 --target openrct2 openrct2-cli -j4
```

For a separate experimental threaded build, pass
`-DOPENRCT2_AMIGA_THREADS=ON` to configure. Do not mix binaries or objects
from different memory profiles. Original RCT2 assets are not included.

## Integration details

Preserve the newer 0.5.6 APIs while importing the 0.5.5 port. The shared
legacy tile accessors decode maze words themselves; the full S4/S6 swap
passes therefore leave those maze bytes untouched. RCT1 checksum detection
uses explicit unaligned little-endian byte assembly. The `MapRange` network
serializer writes its converted bytes directly to avoid double conversion
by the Amiga stream adapter. The configure script consumes its positional
arguments before forwarding extra CMake options.

## Validation recorded during integration

The complete native GUI and CLI cross-build with the 68060 profile. Host
simulation checks match the pre-integration standard profile and our previous
constrained build for Blackpool Pleasure Beach (1000 measured ticks plus 40
warmup) and the car-ride fixture (2000 plus 40): final selected-state
fingerprints `d3ad677c` and `ec449c62`. The actual `.park` importer rejects a
257-tile map. Runtime/save diagnostics and both host job-pool modes pass.
These selected-state checks are not a full multiplayer checksum or proof of
all parks' equivalence.

Integration exposed an exit-order bug in the native allocator: this Hunk
compiler discards the priority in `destructor(101)`, allowing the heap to be
returned before later destructors use it. The cleanup now uses libnix's
`ADD2EXIT(..., -50)`, matching its own allocator's exit stage. The threaded
option additionally enables dlmalloc's blocking pthread locks; the original
inline build retains its single-threaded allocator configuration.

A second native failure was caused by four-byte `AllocMem` alignment in
large allocations: dlmalloc's mmap size/flag encoding assumes an eight-byte
aligned base. The adapter now aligns the returned region, retains the
original Exec pointer and total size, and frees that exact allocation.
Both native executables subsequently pass `--version` with volamos's dirty
heap and sanitizer enabled, without reported violations. The allocator
regression exercises large malloc/realloc/calloc blocks and static lifetime.

Full native simulation under volamos is blocked by its unimplemented
`CreateNewProc`, so no native simulation parity or WinUAE graphics result is
claimed here. The graphical game also needs the user's original RCT2 assets.
The default remains inline jobs; the experimental threaded game has not been
validated on WinUAE. 64 MB graphical playability remains a target to measure.

## Startup prerequisites (0.14 launcher fix)

The shipped `data` drawer contains OpenRCT2's own assets. It does not include
original RCT2 `Data/G1.DAT`. The `RCT2:` assign must name the original game
installation root, with `Data/G1.DAT` and `ObjData` inside it. A missing
assign is checked with `Assign ... EXISTS` before accessing it, avoiding the
system's misleading volume requester. The integer-only `OpenRCT2Check`
checks that the G1 header is readable and reports missing data without
starting the game. This is a prerequisite check, not G1 format validation.

The 0.14 open4 binary also has an FPU requirement in linked runtime code,
despite application `-msoft-float` flags. Its `--version` path succeeds with
an emulated 68060 FPU and fails without one. `OpenRCT2Check` inspects the OS
FPU flags and reports this requirement before C++ static initialisation.
This does not establish the cause of a particular user's crash. The native
checker was tested with a missing G1, truncated G1, synthetic eight-byte
path fixture, FPU present and FPU absent. No actual park/graphics test is
claimed by these fixtures.
