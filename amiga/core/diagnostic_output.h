/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <cstdarg>
#include <cstdio>

static FILE* report;
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
