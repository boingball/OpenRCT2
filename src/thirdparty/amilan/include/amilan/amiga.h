/* Application-provided freestanding Amiga OS bridge. No game/NDK headers.
 * regs: d0-d7, a0-a5; a6 is base, return d0. All pointers/longs are 32 bits
 * on the real m68k ABI. Mock tests exercise the register contract on Linux. */
#ifndef AMILAN_AMIGA_H
#define AMILAN_AMIGA_H
void *os_openlib(const char *name, long version);
void os_closelib(void *base);
long os_call(void *base, long lvo, const long *regs);
#endif

