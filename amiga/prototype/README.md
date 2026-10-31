# OpenRCT2 Amiga imported park viewer 0.9

Targets 68060, 64 MB Fast RAM, AmigaOS 3.1+ and an 8-bit P96/CyberGraphX RTG
mode. This milestone displays **real imported terrain/path data**, with
simplified geometry and colours. It does not simulate the park or display
rides or scenery. Saved guest positions appear as static diagnostic markers.

The 0.9 download includes pre-exported maps, so no G1 data or conversion step
is needed to try the viewer. In its drawer, run:

```text
Protect OpenRCT2ParkView +e
OpenRCT2ParkView MAP=TestPark.amap
OpenRCT2ParkView MAP=BusyPark.amap LOW
```

Use cursor keys to pan, `[` to zoom out, `]` to zoom in, Home/H to centre,
G for the terrain grid, Space to toggle guest markers, and Escape to quit. B starts a ten-second benchmark;
in map view it forces a complete map redraw each frame and measures that plus
RTG copy and frame wait. Normal stationary map viewing caches the background.
Results use the existing `OpenRCT2Runner.bench` filename. This is a graphics
benchmark, with no simulation cost included.

For another classic SV6 save, use a writable drawer:

```text
OpenRCT2ImportProbe MyPark.sv6
OpenRCT2ParkView MAP=ImportedMap.amap
```

The importer writes its report, an overview PPM and the portable `.amap` file.
It reads the source save without modifying it. Terrain, paths and selected metadata enter an isolated partial GameState;
guest positions are separate view-only snapshots, not active engine entities. SC6, modern `.park` and extended Classic saves are not
supported. Files with invalid checksums, bounds or tile chains are rejected.
The viewer independently validates the AMAP structure and checksum before
opening its display; [MAP_FORMAT.md](MAP_FORMAT.md) documents the format.

The map uses diagnostic terrain, water and queue colours. Static gold markers
show saved guest positions, with cyan markers for outside-park guests; riding
and picked-up guests are hidden. BusyPark includes 2,189 valid snapshots.
Unplaced/off-map or invalid records are skipped and counted by the importer.
The saved guest counter can differ from this snapshot count. Markers do not
move or simulate AI, and clothing/animation metadata is not rendered yet.
AMAP v1 terrain-only maps remain supported. Water/terrain and
path connections are approximate rendering, not OpenRCT2's full painter or
object manager. Optional original RCT2 G1 data provides the existing sprite
browser and flat grass sprite at original zoom. No original game assets are
bundled. The previous graphics demo remains available by omitting `MAP=`.

Verification covers host ASan/UBSan, bare 68060 parser/render tests, HUNK
startup/OS ABI/RTG copy and allocation/file cleanup through mocked libraries.
The captured native map viewport matches host preview pixels at both sizes.
The mock is not P96/CGX: this build still needs real WinUAE/Amiga validation.
No native map frame rate is claimed.

## Original graphics demo

# Graphics runner 0.1

First native executable for investigating a classic Amiga port. Targets a
68060, 64 MB or more Fast RAM, AmigaOS 3.1+, and an RTG graphics card using
Picasso96 or CyberGraphX with an available **640 x 480, 8-bit/256-colour mode**.

**This is a standalone graphics prototype, not a playable OpenRCT2 port.**
It adapts OpenRCT2's original G1 bitmap/RLE sprite format to a small C renderer
and displays a fixed 64 x 64 isometric test map with scripted visitors. It does
not link the C++ game core. There are no ride construction tools, economics,
guest AI, saved parks, scenarios, sound, networking or object-DAT loading yet.
The grey paths in the test scene are generated placeholders.

## Run on an Amiga

1. Unpack the archive to a hard disk.
2. Copy **Data/g1.dat from your original RollerCoaster Tycoon 2 installation**
   into the runner's `Data` drawer. RCT2/Triple Thrill Pack data with 29,294
   entries is supported. RCT Classic uses a different sprite order and is
   rejected. The original game's data is not included.
3. Open an Amiga Shell in the drawer and run:

   ```text
   OpenRCT2Runner
   ```

   If your transfer program loses execute permission:

   ```text
   Protect OpenRCT2Runner +e
   OpenRCT2Runner
   ```

The executable also looks for `g1.dat` immediately beside itself. To use a file
elsewhere, including a path with spaces:

```text
OpenRCT2Runner G1="Games:RollerCoaster Tycoon 2/Data/g1.dat" BENCH
```

No data found with the default search starts an explicitly labelled diagnostic
scene using generated shapes. An explicitly supplied missing or invalid file
is an error, rather than silently changing modes. `DIAGNOSTIC` deliberately
starts without game data.

The default view is 640 x 480. A smaller comparison view is available:

```text
OpenRCT2Runner LOW
```

`LOW` renders at 320 x 240, centred on the smallest available RTG mode that fits.
It does not double the pixels. A specific **8-bit RTG** display ID can be used:

```text
OpenRCT2Runner MODEID=0x50011000
```

That ID is an example, not a universal mode. Use your machine's actual display
ID. Native AGA modes and 15/16/24/32-bit-only RTG modes are not supported by this
first runner. Workbench itself can be in a different colour depth; the runner
opens its own 256-colour screen. It leaves the OS and the OS pointer running.
For this first build, launch from the Shell; an icon/ToolTypes interface is not
provided.

## Controls

| Key | Action |
| --- | --- |
| Cursor keys | Pan the view in screen directions |
| Tab | Switch between test map and original sprite browser |
| `[` / `]` | Previous/next page of 12 sprites |
| `+` / `-` | Increase/decrease the scripted crowd, 16 to 1,024 |
| Space | Pause/resume visitor animation |
| H / Home | Centre the view and reset the sprite page |
| G | Toggle grid in the generated diagnostic terrain |
| B | Start/restart a ten-second graphics benchmark |
| Escape | Close and return to Workbench |

The sprite browser draws at original size and clips oversized sprites to
their cells. It does not load the separate ride/scenery OBJDATA files. Primary
visitor clothing remapping is supported; secondary clothing recolouring and
glass/water blend effects are not implemented.

## Benchmark

Start with `BENCH` or press B. Leave it running for ten seconds. Results go to
the Shell and `OpenRCT2Runner.bench` beside the executable. BENCH measures the
current view and crowd; it leaves the runner open afterward. Compare identical
views/crowd sizes. Record CPU/clock, Fast RAM, graphics card, P96/CGX version and
mode with the result. FPS is paced by `WaitTOF`, so it includes the frame wait.

The HUD shows FPS plus the previous frame's drawing and screen-copy times.
Timing uses `timer.device`'s E-clock and does not assume PAL or NTSC timing.
The static ground is cached. Old visitor rectangles are restored before all
visitors are drawn in depth order. Changed 32 x 16 regions are copied to RTG
through `WriteChunkyPixels`; a pan forces a full ground redraw.

**These numbers measure a graphics harness, not full OpenRCT2 performance.**
Scripted visitors do not pay the cost of pathfinding, guest needs, vehicles,
rides or park management. This benchmark cannot establish whether the actual
game simulation is fast enough for a 68060.

## Memory

The executable uses roughly 123 KiB for code/static storage, including a 64 KiB
private stack, plus two 640 x 480 byte buffers (600 KiB). G1 is read into Fast
RAM once, without a second decoded image copy. The complete file is limited to
40 MiB; oversized data or insufficient Fast RAM produces an error. RTG screen
memory and OS overhead are additional. This runner does not allocate the
modern game core's approximately 32 MiB entity array.

## Build and checks

This subdirectory builds independently. The parent CMake project is unchanged.
On Debian/Ubuntu:

```sh
sudo apt-get install gcc-m68k-linux-gnu binutils-m68k-linux-gnu libc6-dev-m68k-cross qemu-user
cd amiga/prototype
make amiga
make test
make test-asan
make test-68k
make check-060
```

The Linux cross compiler is used **freestanding**: the Amiga executable links
neither Linux libc nor Linux system calls. `start.S` supplies AmigaOS startup
and library calls; `elf2hunk.py` converts the linked ELF to an AmigaDOS HUNK
executable. `-m68060 -msoft-float` avoids FPU use. Compiler/tool paths can be
overridden with `CC68K`, `QEMU`, and `OBJDUMP`.

Host tests cover bitmap/RLE clipping, signed offsets, truncation, malformed
runs, transparent pixels, palette byte order, CLI quoting, and incremental
redraw equality with full redraw. Every changed pixel must be marked dirty.
The same tests run on QEMU's 68060 CPU. The test allocator avoids the cross
libc's unsupported 64-bit-multiply calloc; the Amiga executable uses AllocMem.
`check-060` also inspects the final Amiga executable's code for 68060-trapping
64-bit multiply/divide instructions. Address/undefined sanitizers can be run
with `ASAN_OPTIONS=detect_leaks=0 make test-asan` where the environment cannot
run LeakSanitizer.

An optional `make smoke-hunk` check (Python `machine68k` required) relocates and
runs the actual HUNK binary against mock Amiga library vectors. It verifies
startup, register-level library calls, RTG copy rectangles, Escape handling and
cleanup at both resolutions. The mock backend does not validate real RTG
drivers or hardware speed.

For a deterministic host preview:

```sh
make host
build/runner_host build/diagnostic.ppm
build/runner_host build/original-sprites.ppm /path/to/RCT2/Data/g1.dat
```

The host preview exercises the renderer; it is not an Amiga display test.
Neither QEMU CPU tests nor static instruction checks validate a P96/CGX driver
or actual screen/input behaviour. Real Amiga/WinUAE RTG testing remains needed.
Original commercial G1 data has not been available in the development
environment; synthetic binary fixtures and the upstream example sprite are
used for automated validation.

## Next port milestone

Keep this graphics runner isolated while the full game core is brought up
headless with networking, HTTP, scripting, OpenGL and optional codecs disabled.
Measure its actual simulation tick and retained memory on a small park. Audit
the remaining platform, C++ runtime, endian/alignment and library dependencies
before connecting it to an Amiga drawing backend. Nothing in this prototype
claims that stage is already complete.

## Licence and source

GPL-3.0-only, matching OpenRCT2. See the repository's `licence.txt` and
`NOTICE.md`. Original RollerCoaster Tycoon 2 assets remain separate and are not
redistributed here. Complete source/build scripts for this runner are in
`amiga/prototype` of the associated Git commit.
