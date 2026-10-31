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
