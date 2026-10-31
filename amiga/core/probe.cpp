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

// Do not use printf-family formatting here: open4's vfprintf saves FP
// registers even for integer-only formats, defeating -msoft-float.
static void print(const char* format, ...)
{
    char output[1024];
    size_t used = 0;
    auto put = [&](char c) { if (used + 1 < sizeof(output)) output[used++] = c; };
    va_list args;
    va_start(args, format);
    while (*format)
    {
        if (*format != '%') { put(*format++); continue; }
        ++format;
        if (*format == '%') { put(*format++); continue; }
        if (*format == 's')
        {
            const char* text = va_arg(args, const char*);
            while (*text) put(*text++);
            ++format;
            continue;
        }
        unsigned width = 0;
        while (*format >= '0' && *format <= '9') width = width * 10 + (*format++ - '0');
        bool isLong = *format == 'l';
        if (isLong) ++format;
        const unsigned base = *format++ == 'x' ? 16 : 10;
        unsigned long value = isLong ? va_arg(args, unsigned long) : va_arg(args, unsigned);
        char digits[32];
        unsigned count = 0;
        do { digits[count++] = "0123456789abcdef"[value % base]; value /= base; } while (value);
        while (width > count) { put('0'); --width; }
        while (count) put(digits[--count]);
    }
    va_end(args);
    output[used] = 0;
    std::fputs(output, stdout);
    std::fflush(stdout);
    if (report != nullptr)
    {
        std::fputs(output, report);
        std::fflush(report);
    }
}


static void test_threads()
{
    print("Thread stage: construct mutex\n");
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

    print("Thread stage: create worker 1\n");
    std::thread first(worker, 1);
    print("Thread stage: create worker 2\n");
    std::thread second(worker, 2);
    if (!first.joinable() || !second.joinable())
        throw std::runtime_error("std::thread did not create joinable workers");
    print("Thread stage: join workers\n");
    first.join();
    second.join();

    print("Thread stage: condition variable\n");
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
    print("Probe 0.5: entered main (integer-only output)\n");
    report = std::fopen("OpenRCT2CoreProbe.txt", "w");
    print("OpenRCT2 C++23 core ABI/runtime probe 0.5\n");
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
        print("Stage: vector/sort/RNG\n");
        random_engine_t engine(12345);
        std::vector<uint32_t> values;
        for (int i = 0; i < 1024; i++) values.push_back(engine());
        std::sort(values.begin(), values.end());
        uint32_t hash = 2166136261u;
        for (const auto v : values) hash = (hash ^ v) * 16777619u;
        hash ^= ByteSwapBE<uint32_t>(0x12345678u);
        print("Vector/sort/core RNG/endian checksum: %08lx\n", static_cast<unsigned long>(hash));

        print("Stage: exceptions\n");
        bool exceptionCaught = false;
        try { throw std::runtime_error("probe"); }
        catch (const std::runtime_error&) { exceptionCaught = true; }
        if (!exceptionCaught) throw std::runtime_error("C++ exception test failed");

        bool allocate = false;
        bool threads = false;
        for (int i = 1; i < argc; ++i)
        {
            if (std::strcmp(argv[i], "ALLOCATE") == 0) allocate = true;
            else if (std::strcmp(argv[i], "THREADS") == 0) threads = true;
            else throw std::runtime_error("Usage: OpenRCT2CoreProbe [ALLOCATE] [THREADS]");
        }
        if (threads)
        {
            print("Stage: threads (experimental runtime test)\n");
            test_threads();
            print("C++ threads/mutex/condition_variable/atomic/thread_local/call_once: PASS\n");
        }
        else print("Threads: SKIPPED (run THREADS to test separately)\n");

        if (allocate)
        {
            print("Stage: allocation/touch\n");
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
