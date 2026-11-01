# Building OpenRCT2 for AmigaOS 3.x (m68k)

The port cross-compiles on a Unix host with **bebbo's amiga-gcc**, GCC 16
(`amiga16.2` branch). Everything is static and uses the libnix C runtime
(`-mcrt=nix20`); the result is a plain AmigaOS hunk executable.

## 1. Compiler

```sh
git clone https://github.com/bebbo/amiga-gcc.git
cd amiga-gcc
# GCC 16.2 branch; on macOS use Homebrew bison/gmake/bash
make all PREFIX=$HOME/opt/amiga16 GCCBRANCH=amiga16.2 -j8
```

The tree also needs the AHI developer includes (`devices/ahi.h`) in
`$PREFIX/m68k-amigaos/include` for sound; they are part of the AHI
developer archive (Aminet `dev/misc/ahidev_4.16.lha`).

## 2. Dependencies

zlib, zstd, libpng and libzip, static, cross-built (also install the
header-only nlohmann JSON library in the dependency prefix):

```sh
distribution/amiga/build-deps.sh $HOME/amiga-deps
```

## 3. Configure and build

```sh
distribution/amiga/configure.sh build-68k $HOME/amiga-deps
make -C build-68k openrct2 openrct2-cli -j8
```

`AMIGA_CPU_FLAGS` defaults to `-m68020 -msoft-float`, which runs on every
accelerated Amiga, on PiStorm/Emu68 and on the Vampire. There is no 68040
multilib in amiga-gcc, and 68060 code can hit unimplemented instructions on
Emu68, so 68020 is the safe baseline.

Notable toolchain facts the tree works around:

- bebbo's GCC defines `int32_t` as `long`; the toolchain file force-includes
  `cmake/toolchains/m68k-amigaos-int32.h` to make it `int`.
- no position-independent code (hunk format); no `std::thread` (JobPool runs
  inline by default; see [INTEGRATION.md](INTEGRATION.md) for the experimental worker build); libstdc++'s `std::filesystem` is misconfigured, so the port routes
  file operations through `stat()`/`mkdir()` (`core/Path.cpp`, `core/File.cpp`).
- libnix leaves `dirent::d_type` uninitialised (`core/FileScanner.cpp`).

## 4. Packaging

`distribution/amiga/build-archive.sh` stages the tester layout
(`OpenRCT2/{OpenRCT2 launcher, bin/openrct2, data/, user/}`) and packs an
LhA archive. The launcher expects the original RCT2 data under the `RCT2:`
assign; see `TESTER-GUIDE.md`.

## 5. Verifying a change

The simulation is bit-exact with the x86-64/arm64 build. A native build of
the same tree (with `-DOPENRCT2_KEEP_CHECKSUM=ON` and `DISABLE_NETWORK=ON`)
is the oracle: `openrct2-cli simulate <park> <ticks>` prints the entity
checksum on both hosts. Use a `.park` file (or an `.sc4`/`.sc6` at tick 0);
scenario ticks after the first are not deterministic even natively.

Before a build goes out to testers, run the release gate in
[`tools/`](tools/README.md): parity, the release check, the soak and the
leak check, all on the exact binary that ships.

Debug switches (environment variables on the Amiga):

- `OPENRCT2_TRACE=<file>`: opt-in stage trace (start-up, load, frame-rate,
  audio health lines) written to the file; also mirrors console errors.
- `OPENRCT2_AUDIO_DUMP=<file>`: bypass AHI and write the mixed stream
  (16-bit big-endian stereo, 22050 Hz) at real-time pace.
- `OPENRCT2_NO_AUDIO=1`: force the dummy audio context.
- `OPENRCT2_ENTITY_DUMP=<file>`: dump the entity stream after `simulate`
  for cross-host comparison.

### No-FPU runtime selection (0.15)

No FPU is required by the port target. With open4, compiling application
code with `-m68060 -msoft-float` still selects prebuilt `libm060` runtime
archives at link time. Those contain FPU instructions. Keep application
compilation at 68060, but select the base software-float runtime using only
`-m68000 -msoft-float` at final link time. Mixing the CPU aliases still
selects libm060 with this driver, so the toolchain excludes compile FLAGS
from the final executable link template. CRT/soft-float link settings are
explicit through `AMIGA_RUNTIME_CPU_FLAGS`; the configure script also
updates existing linker caches. Additional ABI or LTO flags need explicit
link options when using this template.

For a raw CMake invocation reusing an older build directory, explicitly set
`-DCMAKE_EXE_LINKER_FLAGS="-m68000 -msoft-float -mcrt=nix20"`, or use a fresh
build directory. `CMAKE_EXE_LINKER_FLAGS_INIT` only seeds new caches.

The launcher keeps the original-data check and removes the FPU requirement.
`openrct2-soft-float-test` exercises nonconstant floating-point math, numeric
formatting/parsing, shared pointers and static destruction. Run it and the
native allocator test with the emulated FPU disabled.

Paula output tests (0.21):
```
cmake --build build/nofpu18 --target openrct2-paula-pcm-test
# Host fault injection compiles the actual production Paula worker:
gcc -std=c11 -O1 -g -Wall -Wextra -fsanitize=address,undefined \
  -D__amigaos__ -Iamiga/core/tests/paula_mock \
  amiga/core/tests/paula_lifecycle.c -o /tmp/paula-lifecycle
ASAN_OPTIONS=detect_leaks=0 /tmp/paula-lifecycle
```
The mocked lifecycle test does not validate physical audio.device timing.
Select `OPENRCT2_AUDIO=PAULA` at runtime to bypass AHI completely.

Experimental LAN/thread build (keep separate from the offline binary):
```
AMIGA_CPU_FLAGS='-m68060 -msoft-float' sh distribution/amiga/configure.sh \
  build/experimental22 /path/to/deps /path/to/open4 \
  -DOPENRCT2_SMALL_MEMORY=ON -DDISABLE_NETWORK=OFF \
  -DOPENRCT2_AMIGA_LAN_EXPERIMENT=ON -DOPENRCT2_AMIGA_THREADS=ON \
  -DOPENRCT2_BUILD_CORE_BENCH=ON
cmake --build build/experimental22 --target openrct2 \
  openrct2-job-pool-test openrct2-network-wire-test \
  openrct2-amilan-bridge-test openrct2-network-crypto-test -j4
```
Use an AmiSSL v5 developer SDK in the cross-compiler include directory.
The runtime is opened lazily; no libamisslauto startup dependency is linked.
See amiga/core/tests/{amilan_transport,network_fragments,job_pool}.cpp for
host reference tests. The Amiga bridge test uses an actual synthetic library
vector and the production assembly. The native worker and crypto tests are
included for a real AmigaOS/WinUAE test, not marked passed by a version run.

### Explicit OS tag lists (0.26)

Use `OpenScreenTagList`, `OpenWindowTagList`, `NewObjectA`,
`SetWindowPointerA`, `EasyRequestArgs`, `LockBitMapTagList` and
`CreateNewProc` with explicit arrays. The official NDK 3.2 inline stdarg
wrappers cast `&firstTag` to a tag array. With open4 GCC 16 at `-O3`, the
following varargs values can be discarded: screen creation received only
the first tag followed by unrelated stack contents. `NO_INLINE_STDARG` in
the display/audio units prevents reintroducing these wrappers.

`tools/ndk-tags-check.c` installs a fake Intuition LVO -612 vector that checks
the real A1 argument, without needing a screen implementation. Compile at
`-m68060 -msoft-float -mcrt=nix20 -O3`, then link separately with
`-m68000 -msoft-float -mcrt=nix20`. The normal explicit-array version must
print PASS. Building with `-DTEST_SDK_VARARGS` reproduces FAIL with this SDK
and compiler. This verifies argument delivery, not physical screen creation.

Screen failures now report the failing stage, OS screen error code and
free/largest Chip RAM. A top-level exception handler unwinds owned contexts
and returns 20 to Shell instead of calling abort for a startup exception.
