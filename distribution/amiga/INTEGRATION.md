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

The 0.14 open4 binary incorrectly selected FPU-dependent runtime archives,
despite application `-msoft-float` flags. Requiring an FPU contradicted the
port target. The 0.15 change keeps the application compiled for 68060 and
selects the base 68000 software-float runtime for the final link, removing
the FPU gate from `OpenRCT2Check`. An isolated 68060-compiled math/formatting
smoke test failed when linked against the 68060 runtime and passed when
linked against the base runtime, both with FPU emulation disabled.
The original-data check is retained. Gameplay stability and memory footprint
remain separate from this runtime fix.

### No-FPU validation

The final link rule must omit the application's CPU aliases entirely;
appending `-m68000` after `-m68060` still selected libm060 with open4.
With only the software-float runtime CPU at link time, both full native
executables pass version/startup/shutdown tests under
`volamos --cpu 68060 --no-fpu --dirty-heap --sanitize --ram 64M --stack 1M`.
GUI help, the explicit floating-point math/format/parsing/shared-pointer
regression, the core runtime probe, the allocator/static-lifetime test and
the data checker also exit successfully. No FPSP emulation is enabled.
These are runtime checks, not full GUI gameplay or native simulation parity.
The original RCT2 data requirement and the 64 MB graphical-memory validation
remain outstanding separately.

### Directory lookup and diagnostic launches

`OpenRCT2Check` explicitly enumerates the installation root and its Data
folder, compares entry names without ASCII case sensitivity, and opens the
actual filename spelling returned by DOS. It no longer relies on a previous
CD/Dir visiting the directory. NameFromLock/AddPart provide an absolute
path and the process current directory is never changed. Fresh native
processes with FPU disabled pass upper/lower/mixed-case path fixtures;
missing and truncated G1 files are rejected. These synthetic headers are
path tests, not valid game data or graphical-startup evidence.

`TraceStartup` and `TraceStartup-NoAudio` append an Avail snapshot and
startup stage logging to separate normal/no-audio files on Work:. Console
output goes to a different file to avoid two output handles overwriting
trace entries. The latter launcher temporarily disables audio for an A/B
comparison. Both retain the normal game launcher and existing configuration.
The reported post-screen Guru is not diagnosed or fixed by these helper
changes; its trace is needed to identify the failing stage.

### RTG startup candidate (0.16)

A WinUAE trace reaches a 640x480 depth-8 screen, drawing-buffer resize,
and enumeration of 29 distinct RTG modes, then stops before G1 loading.
The next source-level operation queried the Workbench desktop mode even
though Amiga aspect filtering accepts every valid RTG mode and never needs
that result. The Amiga branch now omits the desktop query and aspect-ratio
floating-point calculations; other platforms retain their aspect filter.
Failed per-mode queries are ignored. Additional opt-in trace checkpoints
cover collection/sort/storage, fullscreen application, window completion,
user folders, audio and original-user-file copying.

An isolated harness compiled the actual updated resolution method and
exercised 29 inputs (duplicates, failed/invalid modes and widescreen),
sorting, default selection and preservation of explicit settings. It
asserts the desktop query is never called. The native harness and full
candidate version/startup/shutdown pass with FPU disabled, dirty heap and
sanitizer. Single-precision aspect division also passes independently.
The real WinUAE Guru has not been reproduced here; desktop-query bypass is
a targeted candidate, not a confirmed diagnosis or full-game validation.

### Single-precision runtime repair (0.17)

The next WinUAE trace passes resolution collection and fullscreen setup,
then stops inside TriggerResize. Its first maths call is floorf(windowScale).
Disassembly of the delivered 0.16 executable confirms that open4's base-libm
floorf, ceilf and sqrtf each call themselves directly with no termination.
An isolated native floor(float) probe also fails to return. This is a
runtime recursion defect, not evidence of an FPU opcode in those functions.

The open4 compatibility layer now wraps all three symbols and calls their
software double equivalents through volatile function pointers, preventing
GCC from folding the adapter back into the broken float symbol. Forced
archive extraction applies the repair to GUI, CLI and linked diagnostics.
Extra resize checkpoints distinguish scale quality, dimensions and resize
completion. The regression exercises actual float overloads and C symbols;
the previous double-only checks could not detect this defect.

The isolated repaired floor probe and actual TriggerResize method with mock
SDL/OnResize pass on a 68060 with FPU disabled, dirty heap and sanitizer.
The full native runtime regression (including negative zero, infinities and
NaNs) and the rebuilt GUI version/startup/shutdown check also pass under
those settings, with 64 MB RAM and a 1 MB stack and without FPSP emulation.
Real WinUAE startup beyond this point and 64 MB gameplay remain unverified.

### Launcher option quoting (0.17a)

The next real WinUAE trace confirms window setup, TriggerResize, audio
initialisation and context shutdown complete. G1 loading now reports an
attempt to open '"RCT2:"/Data/g1.dat', while the standalone prerequisite
checker resolves UHD0:RCT2/Data/g1.dat successfully. The launcher embedded
quotes after an option's equals sign; the native CRT preserved those quotes
in the argument value. This caused the bogus volume request and G1 failure.

Pass each option and its quoted value as separate arguments, which the
engine's ParseLongOption explicitly supports. This also retains paths with
spaces. The 0.17a update changes only the launch script and requires the
existing 0.17 executable. It does not establish successful G1 loading or
gameplay until the corrected launch has been tested on WinUAE.

### S6 profile validation and title background (0.18)

WinUAE now indexes 57 scenarios and 204 tracks, but every attempted park and
both title-sequence parks fail the small-memory map-size check. That check
read the raw little-endian MapSize before ByteSwapS6IfBigEndian. Validation
now runs after conversion and retains the 5..256 tile boundary.

The native regression also exposed an integration conflict: ByteSwapBE now
converts to big-endian and is therefore the identity on Amiga, while our
ToLittleEndian helper still called it expecting unconditional reversal.
ToLittleEndian now uses ByteSwapT directly on big-endian hosts. Fixed-byte
tests cover 16/32/64-bit values, enums, reading and writing. The previous
header fails the native fixed-byte test; the corrected header passes both
native no-FPU and little-endian host runs. Round trips alone would not
detect matching encoder/decoder errors.
The integrated IStream already converts ReadValue/WriteValue values. Remove
our redundant Sawyer chunk-length conversion after ReadValue and the track
checksum conversion before WriteValue, so each is converted exactly once.

The Amiga main window also clears each dirty title-background region before
rendering its viewport. An unloaded title map does not otherwise paint every
pixel, leaving old menus/tooltips in the framebuffer. This adds no forced
full-screen invalidation and does not clear normal gameplay viewports.

The S6 regression generates wire-order Sawyer streams for both the split
SC6 tail and the SV6 whole tail, using the real importer and empty object
slots. It tests sizes 0, 4, 5, 150, 256 and 257; optional file input tests
decoding and required-object extraction without importing or simulating.
All twelve generated-stream cases pass with `--cpu 68060 --no-fpu
--dirty-heap --sanitize --ram 64M --stack 1M`, without FPSP emulation.
The fixed track output/checksum bytes, software-float runtime regression and
rebuilt GUI version/startup/shutdown also pass. A real saved-file fixture
stops at a volamos continuation-stub error; it is not a successful load or
a diagnosed game failure, and full native import/simulation remains open.
Graphical confirmation and full-game memory validation remain separate.

### Magnified RLE sprites (0.19)

The next WinUAE log and tester report confirm the title park and Electric
Fields now load and run. The two closest zoom levels corrupt graphics.
The RLE magnification path (-1/-2, or 2x/4x) copied the row offset directly
into a native uint16_t, unlike the regular/minified path which explicitly
decodes little-endian row offsets. Decode the two bytes explicitly in the
magnification path too, retaining unaligned-data safety on m68k.

The regression compiles the actual RLE renderer and compares its complete
output buffer to an independent dense-pixel reference. Twenty-four cases
cover 1x/2x/4x, clipped source coordinates, gaps, remap transparency, source
and destination blending, a padded destination stride and untouched guard
pixels. Row offsets exceed 255, have distinct high/low bytes and use an
unaligned sprite base. The previous native renderer fails; the fixed
renderer passes native no-FPU/dirty-heap/sanitizer and little-endian host
runs. The rebuilt GUI version/startup/shutdown also passes. Real WinUAE
visual confirmation at 2x/4x remains required.

Memory remains above the original target: the tester's running game reports
roughly 90-107 MiB of heap footprint before executable/OS costs. A 64 MB
playable release is not established by the capacity profile or these tests.

### Experimental native AGA display (0.20)

`OPENRCT2_DISPLAY=AGA` selects a fixed PAL 640x512 interlaced, 256-colour
screen. `OpenRCT2-AGA` sets it for the normal launch script and unsets it
after a normal return. Without that selection the existing RTG backend is
used. AGA selection bypasses cybergraphics.library opening and all RTG mode
queries. It requires an available PAL hires-lace mode supporting eight
planes; unavailable modes fail rather than silently selecting RTG.

The custom bitmap has eight non-interleaved planes in Chip RAM.
Its depth, stride/layout and Chip placement are checked. The OS owns the
screen/copper handling; the supplied bitmap is freed after screen closure.
Two contiguous Fast-RAM blocks hold working planes and a shadow of the
display. At 640x512 these are 320 KiB each, plus a 320 KiB Chip bitmap.
The existing chunky framebuffer remains in Fast RAM.

The converter adapts Ami3D's `src/c2p.c` transpose to all eight planes,
retaining its full 256-colour palette. Aligned 32-pixel groups use the
transpose; partial edges preserve surrounding bits without reading outside
the source rectangle. Only dirty regions are converted. Following Ami3D's
PLANAR5FS approach, only longwords differing from the screen shadow are
written to Chip RAM. Startup copies the actual Intuition backdrop into both
Fast copies so the first partial update is also coherent.

The independent pixel-reference test covers all 256 indices, a complete PAL
frame, 130 partial updates, source/destination padding, guard bytes and zero
writes on unchanged presentations. It passes host ASan/UBSan (leak checking
disabled for this container) and native 68060/no-FPU/dirty-heap/sanitizer.
These conversion tests do not exercise a real custom screen, copper or
display DMA. The prototype is single buffered; interlace flicker and tearing
are possible. AGA WinUAE/hardware validation and speed measurements remain
required. Use the working RTG test's RAM allocation initially; this does not
establish that the full game fits 128 MB or the original 64 MB target.

### Paula output without AHI (0.21)

The default output tries AHI and falls back to built-in Paula if it cannot
open. `OPENRCT2_AUDIO=PAULA` selects Paula without probing/opening AHI;
`OPENRCT2_AUDIO=AHI` retains AHI-only behaviour. `OpenRCT2-Paula` and
`OpenRCT2-AGA-Paula` select the output for their respective display launcher.
After a failed launch, unset OPENRCT2_AUDIO before returning to automatic
selection. OPENRCT2_NO_AUDIO still disables sound.

The existing 22050 Hz, signed 16-bit stereo mixer and WAV/CSS streaming
remain. The playback process splits its high sample bytes into signed
8-bit Chip-RAM stereo planes. audio.device owns/arbitrates channels 0/1;
the code follows MintAMP's two queued writes per channel and BeginIO
approach. It does not take over hardware, alter the audio filter or require
an AHI installation. The Paula period is rounded from the machine's E-clock
(PAL/NTSC), so the output rate is approximate. This first version is 8-bit,
not a calibrated 14-bit Paula output. It needs an extra 8 KiB Chip RAM.

Shutdown aborts every submitted request and waits for its reply before
closing the device or freeing DMA buffers. A host test exercises the actual
Paula worker against mocked Exec/device calls: normal queue/stop/reap/free,
every allocation failure and device-open failure. ASan/UBSan passes. The
PCM reference test covers all high-byte values, distinct stereo channels,
unaligned source and output guards, including native no-FPU testing.
These tests do not establish audible playback, hardware timing or AGA/audio
contention. WinUAE listening and real hardware testing remain required.

Ogg decoding is still disabled for this Amiga build. Original RCT2 WAV/CSS
music/effects do not require it. Classic assets and Ogg custom music would
need a decoder. MintAMP uses fixed-point Tremor, but its sequential adapter
does not implement the seek/loop behaviour required here; do not transplant
that adapter unchanged or enable floating-point libvorbis for this target.

### Experimental job workers and LAN (0.22)

This is a separate executable/profile for testing; do not replace the 0.21
fallback. Configure OPENRCT2_AMIGA_THREADS=ON, DISABLE_NETWORK=OFF and
OPENRCT2_AMIGA_LAN_EXPERIMENT=ON, retaining OPENRCT2_SMALL_MEMORY=ON and
68060/software-float flags. OPENRCT2_AMIGA_THREADS enables blocking allocator
locks and caps JobPool at two 256-KiB workers. The Multithreading checkbox
then enables actual viewport jobs, including the X8 parallel drawing path.
Other job-pool users (such as loading) can use workers independently of the
checkbox. No multicore 68060 speedup is promised.

The socket interface delegates raw transport to the AmiLAN Amiga backend.
OpenRCT2 retains its existing protocol, RSA authentication, map transfer,
commands, tick checksums and LAN discovery datagrams. No AmiLAN framing or
512-byte message limit is imposed on OpenRCT2's stream. Numeric IPv4 only;
no DNS/IPv6, internet master listing, HTTP object downloads or scripting.
The socket API is owned by the OS task using it. Accepted sockets share
the listener's library base; all descriptors close before the base.

The constrained build appends -amiga-small-v1 to the existing network version
so standard/Thomas builds cannot inadvertently join with different caps.
This is a compatibility gate, not proof of determinism. Test two identical
0.22 builds with a small park first. Existing map/entity/ride/tile caps remain.

AmiSSL v5/OpenSSL 3 API is opened lazily for OpenRCT2 RSA operations. Its
entropy and RSA checks remain in place; no authentication bypass is added.
The RCT1 patch SHA256 uses the existing picosha2 path on Amiga so ordinary
single-player patching does not require AmiSSL. Crypto cleanup occurs after
C++ teardown and before allocator cleanup. The SDK headers are needed at
build time; an installed compatible AmiSSL runtime is needed for networking.

Host ASan/UBSan checks cover the actual job pool, real TCP 1 MiB partial I/O,
accepted-socket lifetime, EOF, invalid address, raw UDP and the actual packet
serializer/parser using two-byte writes and one-byte reads. That last check
also covers a repaired short-header read in Connection::readPacket().
Native no-FPU checks pass fixed Packet bytes, the actual assembly library
bridge and GUI version/startup/shutdown. The native worker test is blocked
by volamos's unimplemented CreateNewProc. The crypto test reports missing
AmiSSL there. Audible audio, graphical worker stability, live AmiSSL/RSA,
Amiga sockets, join/map transfer and sustained synchronisation require
WinUAE/hardware tests; multiplayer is not yet validated end to end.

On Amiga the client opens TCP after generating/loading its local key, so a
slow first-time RSA generation does not consume the server handshake timeout.
Key generation failure stops the join; it is not treated as a valid identity.

### 0.22.1: keep file loading on its owning task

A WinUAE tester reports most objects missing/corrupt when loading Crazy Castle
with 0.22. That build enabled workers in ObjectManager and FileIndex as well
as viewport rendering. The screenshot alone does not establish the exact
failure (DOS task context, shared object-loader state or another issue).

On Amiga, ObjectManager loading and FileIndex building now explicitly request
JobPool(0), retaining the established main-task file I/O path. Experimental
viewport workers remain enabled. The threaded Amiga file-index format version
is bumped to force rebuilding potentially incomplete caches from 0.22. This
changes caches only, not park saves, network protocol or capacity limits.
The same user-experimental22 profile is retained. Trace records both the
main-task loader/index path and separate viewport worker creation.

The actual JobPool regression checks explicit zero-worker dispatch/completion
on the caller, then the two-worker path. WinUAE park loading/rendering still
requires tester confirmation; this is a targeted isolation fix, not a claim
that threaded Amiga file I/O has been repaired.

The follow-up WinUAE console confirms OpenRCT2ThreadTest PASS for actual
workers. It also shows absolute object.json paths opening with nonzero file
sizes but reads failing at position zero with EOF set and no stream error.
This supports isolating file I/O from workers; it is not a missing-file
report and does not yet identify the runtime's precise stdio/task defect.
