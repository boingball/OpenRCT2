/* SPDX-License-Identifier: GPL-3.0-only */
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>

extern "C" int __wrap_snprintf(char*, size_t, const char*, ...);

// open4's pthread_create uses this exact format before launching a worker.
// Keep that integer-only operation out of libnix's FPU formatter. All other
// snprintf calls retain their normal semantics through vsnprintf.
extern "C" int __wrap_snprintf(char* buffer, size_t size, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    if (std::strcmp(format, "pthread thread #%ld") != 0)
    {
        int result = std::vsnprintf(buffer, size, format, args);
        va_end(args);
        return result;
    }
    const long id = va_arg(args, long);
    va_end(args);
    char text[48] = "pthread thread #";
    size_t length = sizeof("pthread thread #") - 1;
    unsigned long value = static_cast<unsigned long>(id);
    if (id < 0) { text[length++] = '-'; value = 0UL - value; }
    char digits[32];
    unsigned count = 0;
    do { digits[count++] = '0' + value % 10; value /= 10; } while (value);
    while (count) text[length++] = digits[--count];
    if (size != 0)
    {
        const size_t copied = std::min(size - 1, length);
        std::memcpy(buffer, text, copied);
        buffer[copied] = 0;
    }
    return static_cast<int>(length);
}
