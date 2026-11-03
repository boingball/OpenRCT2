/* SPDX-License-Identifier: GPL-3.0-only */
// Exercise the actual Exec-backed allocator, including static destruction.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static std::vector<uint8_t> first(786444, 0x3c);
static std::vector<uint8_t> second(1048576, 0x5a);

int main()
{
    for (size_t round = 0; round < 32; ++round)
    {
        // Interleave small blocks so AllocMem's next address can be 4 mod 8.
        std::vector<uint8_t> small(17 + round, 0x71);
        const size_t count = 262144 + 13 * round;
        auto* block = static_cast<uint8_t*>(std::malloc(count));
        assert(block != nullptr && reinterpret_cast<uintptr_t>(block) % 8 == 0);
        std::memset(block, 0xa5, count);
        auto* resized = static_cast<uint8_t*>(std::realloc(block, count + 8193));
        assert(resized != nullptr && reinterpret_cast<uintptr_t>(resized) % 8 == 0);
        for (size_t i = 0; i < count; ++i)
            assert(resized[i] == 0xa5);
        std::free(resized);
        auto* zero = static_cast<uint8_t*>(std::calloc(count, 1));
        assert(zero != nullptr && reinterpret_cast<uintptr_t>(zero) % 8 == 0);
        for (size_t i = 0; i < count; ++i)
            assert(zero[i] == 0);
        std::free(zero);
    }
    for (auto byte : first) assert(byte == 0x3c);
    for (auto byte : second) assert(byte == 0x5a);
    std::puts("Native allocator alignment/realloc/calloc/static lifetime: PASS");
    // first and second deliberately survive main, testing allocator exit order.
}
