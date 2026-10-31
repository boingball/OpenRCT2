/* SPDX-License-Identifier: GPL-3.0-only
 * Compile against actual engine headers on the target ABI; no stand-in structs.
 * This executable probes the runtime/memory layout, not the game simulation.
 */
#include <openrct2/GameState.h>
#include <openrct2/MemoryProfile.h>
#include <openrct2/core/Endianness.h>
#include <openrct2/core/Random.hpp>

#include <algorithm>
#include <atomic>
#include <bit>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <thread>
#include <vector>

#ifndef _GLIBCXX_HAS_GTHREADS
#error "This probe requires an Amiga C++ runtime built with POSIX thread support."
#endif

#ifdef __amigaos__
extern "C"
{
    unsigned long __stack = 262144;
    void __stkinit(void);
    void (*keep_stack_init)(void) = __stkinit;
}
#endif

static FILE* report;
static thread_local int threadLocalMarker = 0;

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

static void test_threads()
{
    std::mutex mutex;
    int protectedCounter = 0;
    std::atomic<int> completed{ 0 };
    std::atomic<int> onceCount{ 0 };
    std::once_flag once;
    int threadLocalResults[2] = { -1, -1 };

    auto worker = [&](int id) {
        threadLocalMarker = id;
        for (int i = 0; i < 1000; ++i)
        {
            std::lock_guard<std::mutex> lock(mutex);
            ++protectedCounter;
        }
        std::call_once(once, [&] { onceCount.fetch_add(1); });
        threadLocalResults[id - 1] = threadLocalMarker;
        completed.fetch_add(1);
    };

    std::thread first(worker, 1);
    std::thread second(worker, 2);
    if (!first.joinable() || !second.joinable())
        throw std::runtime_error("std::thread did not create joinable workers");
    first.join();
    second.join();

    std::mutex waitMutex;
    std::condition_variable condition;
    bool ready = false;
    bool woke = false;
    std::thread waiter([&] {
        std::unique_lock<std::mutex> lock(waitMutex);
        condition.wait(lock, [&] { return ready; });
        woke = true;
    });
    {
        std::lock_guard<std::mutex> lock(waitMutex);
        ready = true;
    }
    condition.notify_one();
    waiter.join();

    if (protectedCounter != 2000 || completed.load() != 2 || onceCount.load() != 1
        || threadLocalResults[0] != 1 || threadLocalResults[1] != 2
        || threadLocalMarker != 0 || !woke)
        throw std::runtime_error("C++ thread/mutex/condition-variable test failed");
}

int main(int argc, char** argv)
{
    using namespace OpenRCT2;
    report = std::fopen("OpenRCT2CoreProbe.txt", "w");
    print("OpenRCT2 C++23 core ABI/runtime probe 0.3\n");
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
        hash ^= ByteSwapBE<uint32_t>(0x12345678u);
        print("Vector/sort/core RNG/endian checksum: %08lx\n", static_cast<unsigned long>(hash));

        bool exceptionCaught = false;
        try { throw std::runtime_error("probe"); }
        catch (const std::runtime_error&) { exceptionCaught = true; }
        if (!exceptionCaught) throw std::runtime_error("C++ exception test failed");

        test_threads();
        print("C++ threads/mutex/condition_variable/atomic/thread_local/call_once: PASS\n");

        if (argc == 2 && std::strcmp(argv[1], "ALLOCATE") == 0)
        {
            // Reserve/touch the same number of bytes as GameState, without constructing the game.
            std::unique_ptr<unsigned char[]> block(new (std::nothrow) unsigned char[sizeof(GameState_t)]);
            if (!block) throw std::runtime_error("Cannot reserve a GameState-sized block");
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
