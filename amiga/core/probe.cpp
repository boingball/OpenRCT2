/* SPDX-License-Identifier: GPL-3.0-only
 * Compile against actual engine headers on the target ABI; no stand-in structs.
 * This executable probes the runtime/memory layout, not the game simulation.
 */
#include <openrct2/GameState.h>
#include <openrct2/MemoryProfile.h>
#include <openrct2/core/Endianness.h>
#include <openrct2/core/Random.hpp>

#include <algorithm>
#include <bit>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <vector>

#ifdef __amigaos__
extern "C"
{
    #include <exec/memory.h>
    #include <proto/exec.h>
    unsigned long __stack = 262144;
    void __stkinit(void);
    void (*keep_stack_init)(void) = __stkinit;
}
#endif

static FILE* report;
static void print(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    std::vprintf(format, args);
    va_end(args);
    if (report != nullptr)
    {
        va_start(args, format);
        std::vfprintf(report, format, args);
        va_end(args);
    }
}

int main(int argc, char** argv)
{
    using namespace OpenRCT2;
    report = std::fopen("OpenRCT2CoreProbe.txt", "w");
    print("OpenRCT2 C++20 core ABI/runtime probe 0.2\n");
    print("This is a memory/runtime probe, not the park simulation.\n");
    print("Profile: %s\nPointer bits: %lu\n", MemoryProfile::kSmall ? "small-memory" : "standard",
        static_cast<unsigned long>(sizeof(void*) * 8));
    if constexpr (std::endian::native != std::endian::little)
        print("Full engine byte-order gate: BLOCKED (upstream rejects big-endian targets)\n");
    print("GameState bytes: %lu\nEntityRegistry bytes: %lu\nRide bytes: %lu\n",
        static_cast<unsigned long>(sizeof(GameState_t)), static_cast<unsigned long>(sizeof(EntityRegistry)),
        static_cast<unsigned long>(sizeof(Ride)));
    print("Entity slots: %u\nRide slots: %u\nMap technical size: %u\nTile element cap: %lu\n",
        static_cast<unsigned>(MemoryProfile::kEntities), static_cast<unsigned>(MemoryProfile::kRides),
        static_cast<unsigned>(MemoryProfile::kMapSize), static_cast<unsigned long>(MemoryProfile::kTileElements));
    int result = 0;
    try
    {
        random_engine_t engine(12345);
        std::vector<uint32_t> values;
        for (int i = 0; i < 1024; i++) values.push_back(engine());
        std::sort(values.begin(), values.end());
        uint32_t hash = 2166136261u;
        for (const auto v : values) hash = (hash ^ v) * 16777619u;
#ifdef _GLIBCXX_HAS_GTHREADS
        std::mutex mutex;
        { std::lock_guard lock(mutex); hash ^= ByteSwapBE<uint32_t>(0x12345678u); }
        print("C++ mutex support: available\n");
#else
        hash ^= ByteSwapBE<uint32_t>(0x12345678u);
        print("C++ mutex/thread support: UNAVAILABLE in this toolchain build\n");
        print("Full game core needs a threaded runtime or a single-thread port.\n");
#endif
        bool exceptionCaught = false;
        try { throw std::runtime_error("probe"); }
        catch (const std::runtime_error&) { exceptionCaught = true; }
        if (!exceptionCaught) throw std::runtime_error("C++ exception test failed");
        print("Vector/sort/core RNG/endian/exception checksum: %08lx\n", static_cast<unsigned long>(hash));
#ifdef __amigaos__
        print("Free Fast RAM bytes: %lu\nLargest Fast block bytes: %lu\n",
            static_cast<unsigned long>(AvailMem(MEMF_FAST)), static_cast<unsigned long>(AvailMem(MEMF_FAST | MEMF_LARGEST)));
#endif
        if (argc == 2 && std::strcmp(argv[1], "ALLOCATE") == 0)
        {
            // Reserve/touch the same number of bytes as GameState, without pretending to construct the game.
            std::unique_ptr<unsigned char[]> block(new (std::nothrow) unsigned char[sizeof(GameState_t)]);
            if (!block) throw std::runtime_error("Cannot reserve a GameState-sized block");
            // Volatile stores ensure the compiler cannot elide the memory touch.
            auto* touched = static_cast<volatile unsigned char*>(block.get());
            for (size_t i = 0; i < sizeof(GameState_t); i++) touched[i] = 0x5a;
            uint32_t sum = 0;
            for (size_t i = 0; i < sizeof(GameState_t); i += 4096) sum += block[i];
            print("GameState-sized reserve/touch: PASS (%lu bytes, checksum %lu)\n",
                static_cast<unsigned long>(sizeof(GameState_t)), static_cast<unsigned long>(sum));
        }
        print("C++ runtime checks: PASS\n");
    }
    catch (const std::exception& error)
    {
        print("FAIL: %s\n", error.what());
        result = 20;
    }
    if (report != nullptr && std::fclose(report) != 0) result = 20;
    return result;
}
