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
