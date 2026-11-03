/* SPDX-License-Identifier: GPL-3.0-only */
#include <array>
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstring>

extern "C" int __wrap_snprintf(char*, size_t, const char*, ...);

int main()
{
    // Match snprintf's return value, truncation, NUL and bounds, including
    // the signed minimum (which cannot safely be negated as a signed long).
    for (long id : { 0L, 1L, 2018L, -1L, LONG_MIN, LONG_MAX })
    {
        for (size_t size : { 0U, 1U, 3U, 16U, 64U })
        {
            std::array<char, 80> actual, expected;
            actual.fill('@');
            expected.fill('@');
            int a = __wrap_snprintf(actual.data(), size, "pthread thread #%ld", id);
            int b = std::snprintf(expected.data(), size, "pthread thread #%ld", id);
            assert(a == b && actual == expected);
        }
        assert(__wrap_snprintf(nullptr, 0, "pthread thread #%ld", id)
            == std::snprintf(nullptr, 0, "pthread thread #%ld", id));
    }
    char actual[80], expected[80];
    assert(__wrap_snprintf(actual, sizeof actual, "%s:%08x", "fallback", 123U)
        == std::snprintf(expected, sizeof expected, "%s:%08x", "fallback", 123U));
    assert(std::strcmp(actual, expected) == 0);
    std::puts("pthread name formatting: PASS");
}
