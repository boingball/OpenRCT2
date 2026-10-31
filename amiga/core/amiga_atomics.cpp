/* SPDX-License-Identifier: GPL-3.0-only */
// OpenAmigaGCC open4's libstdc++ atomicity helpers use CAS.L. Its m68k ABI
// permits a shared_ptr reference count at a two-byte-aligned address, which
// faults on 68060. These task-context helpers preserve that ABI without CAS.
// AmigaOS on these targets has one CPU; Forbid/Permit protect the tiny
// read/modify/write against task switches. No allocation or OS wait occurs
// while forbidden. These helpers are not for interrupt-context callers.
#ifdef __amigaos__
#include <ext/atomicity.h>

#ifdef _GLIBCXX_ATOMIC_WORD_BUILTINS
#error "Recheck the atomicity adapter against this toolchain's inline atomics"
#endif

static inline void forbid()
{
    register void* execBase asm("a6");
    asm volatile("move.l 4.w,%0" : "=r"(execBase) : : "memory");
    asm volatile("jsr -132(a6)" : "+r"(execBase) : : "d0", "d1", "a0", "a1", "cc", "memory");
}

static inline void permit()
{
    register void* execBase asm("a6");
    asm volatile("move.l 4.w,%0" : "=r"(execBase) : : "memory");
    asm volatile("jsr -138(a6)" : "+r"(execBase) : : "d0", "d1", "a0", "a1", "cc", "memory");
}

namespace __gnu_cxx
{
    _Atomic_word __exchange_and_add(volatile _Atomic_word* address, int delta) noexcept
    {
        static_assert(sizeof(_Atomic_word) == sizeof(unsigned));
        forbid();
        auto* bits = reinterpret_cast<volatile unsigned*>(address);
        unsigned previous = *bits;
        *bits = previous + static_cast<unsigned>(delta);
        permit();
        return static_cast<_Atomic_word>(previous);
    }

    void __atomic_add(volatile _Atomic_word* address, int delta) noexcept
    {
        (void)__exchange_and_add(address, delta);
    }
}
#endif
