/* SPDX-License-Identifier: GPL-3.0-only */
// Headless diagnostic adapter for Guard's UI-dependent assertion reporting.
// Stream, compression and save decoding use the real engine implementations.
#include <openrct2/core/Guard.hpp>
#include <stdexcept>
namespace OpenRCT2::Guard
{
    void Assert(bool expression, const std::source_location&)
    {
        if (!expression) throw std::runtime_error("Engine stream invariant failed");
    }
    void Assert(bool expression, const char*, ...)
    {
        if (!expression) throw std::runtime_error("Engine diagnostic invariant failed");
    }
    void Assert_VA(bool expression, const char*, va_list)
    {
        if (!expression) throw std::runtime_error("Engine stream allocation failed");
    }
}
