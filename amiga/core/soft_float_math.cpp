/* SPDX-License-Identifier: GPL-3.0-only */
#include <cmath>

extern "C" float __wrap_floorf(float value);
extern "C" float __wrap_ceilf(float value);
extern "C" float __wrap_sqrtf(float value);

// open4's base libm implements these float entry points as direct self-calls.
// Route every external reference through the working software double routine.
// The volatile pointer prevents GCC from folding float(double_function(float))
// straight back into the broken float entry point, even in an optimised build.
extern "C" float __wrap_floorf(float value)
{
    if (value == 0.0f)
        return value;
    double (*volatile function)(double) = ::floor;
    return static_cast<float>(function(static_cast<double>(value)));
}

extern "C" float __wrap_ceilf(float value)
{
    double (*volatile function)(double) = ::ceil;
    const float result = static_cast<float>(function(static_cast<double>(value)));
    // The base double ceil also loses the sign of zero for negative inputs.
    return result == 0.0f ? __builtin_copysignf(result, value) : result;
}

extern "C" float __wrap_sqrtf(float value)
{
    if (value == 0.0f)
        return value;
    double (*volatile function)(double) = ::sqrt;
    return static_cast<float>(function(static_cast<double>(value)));
}
