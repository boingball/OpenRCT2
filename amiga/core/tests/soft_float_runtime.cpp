/* SPDX-License-Identifier: GPL-3.0-only */
// Run this exact native binary with the emulated FPU disabled.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <limits>
#include <string>
#include <vector>

static std::vector<std::string> strings(32, "software float");
int main()
{
    volatile double input = 1.25;
    const double value = input;
    assert(std::fabs(std::sqrt(value * value) - value) < 0.000001);
    assert(std::fabs(std::sin(value) - 0.9489846193555862) < 0.000001);
    assert(std::round(value) == 1 && std::trunc(-value) == -1);
    // These overloads call floorf/ceilf/sqrtf, unlike the double checks above.
    // Exercise runtime arguments through both overloads and actual C symbols.
    volatile float inputs[] = { 1.0f, 1.5f, -1.5f, 0.0f, -0.0f, 8388608.0f };
    const float floors[] = { 1, 1, -2, 0, -0.0f, 8388608.0f };
    const float ceils[] = { 1, 2, -1, 0, -0.0f, 8388608.0f };
    float (*volatile floorFunction)(float) = ::floorf;
    float (*volatile ceilFunction)(float) = ::ceilf;
    float (*volatile sqrtFunction)(float) = ::sqrtf;
    for (size_t i = 0; i < std::size(floors); ++i)
    {
        assert(std::floor(inputs[i]) == floors[i]);
        assert(std::ceil(inputs[i]) == ceils[i]);
        assert(floorFunction(inputs[i]) == floors[i]);
        assert(ceilFunction(inputs[i]) == ceils[i]);
    }
    volatile float square = 2.25f;
    assert(std::sqrt(square) == 1.5f && sqrtFunction(square) == 1.5f);
    volatile float minusZero = -0.0f;
    assert(__builtin_signbit(floorFunction(minusZero)));
    assert(__builtin_signbit(ceilFunction(minusZero)));
    assert(__builtin_signbit(sqrtFunction(minusZero)));
    volatile float negativeFraction = -0.5f;
    assert(ceilFunction(negativeFraction) == 0.0f && __builtin_signbit(ceilFunction(negativeFraction)));
    volatile float infinity = std::numeric_limits<float>::infinity();
    assert(floorFunction(infinity) == infinity && ceilFunction(-infinity) == -infinity);
    assert(sqrtFunction(infinity) == infinity);
    volatile float nan = std::numeric_limits<float>::quiet_NaN();
    assert(__builtin_isnan(floorFunction(nan)) && __builtin_isnan(ceilFunction(nan)) && __builtin_isnan(sqrtFunction(nan)));
    std::puts("Single-precision floor/ceil/sqrt overloads and C symbols: PASS");
    assert(std::fabs(std::stod("1.25") - value) < 0.000001);
    char text[128];
    int count = std::snprintf(text, sizeof(text), "%s %d %.2f", strings[0].c_str(), -7, value);
    assert(count == 22 && std::strcmp(text, "software float -7 1.25") == 0);
    auto first = std::make_shared<std::string>(text);
    auto second = first;
    first.reset();
    assert(second && *second == text);
    std::puts("Software-float math/format/parsing/shared_ptr/static lifetime: PASS");
}
