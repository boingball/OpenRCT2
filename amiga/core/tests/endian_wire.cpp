/* SPDX-License-Identifier: GPL-3.0-only */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <openrct2/core/Endianness.h>
#include <cassert>
#include <cstdio>

enum class TestValue : uint32_t { sample = 0x12345678 };

int main()
{
    const uint8_t wire16[] = { 0x34, 0x12 };
    const uint8_t wire32[] = { 0x78, 0x56, 0x34, 0x12 };
    const uint8_t wire64[] = { 0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01 };
    volatile uint16_t n16 = 0x1234;
    volatile uint32_t n32 = 0x12345678;
    volatile uint64_t n64 = 0x0123456789ABCDEF;
    auto a = ToLittleEndian(static_cast<uint16_t>(n16));
    auto b = ToLittleEndian(static_cast<uint32_t>(n32));
    auto c = ToLittleEndian(static_cast<uint64_t>(n64));
    auto e = ToLittleEndian(TestValue::sample);
    assert(std::memcmp(&a, wire16, sizeof(a)) == 0);
    assert(std::memcmp(&b, wire32, sizeof(b)) == 0);
    assert(std::memcmp(&c, wire64, sizeof(c)) == 0);
    assert(std::memcmp(&e, wire32, sizeof(e)) == 0);
    assert(ReadLittleEndian<uint16_t>(wire16) == n16);
    assert(ReadLittleEndian<uint32_t>(wire32) == n32);
    assert(ReadLittleEndian<uint64_t>(wire64) == n64);
    uint8_t written[8];
    WriteLittleEndian(written, static_cast<uint64_t>(n64));
    assert(std::memcmp(written, wire64, sizeof(written)) == 0);
    std::puts("Little-endian fixed wire bytes: 16/32/64-bit and enum PASS");
}
