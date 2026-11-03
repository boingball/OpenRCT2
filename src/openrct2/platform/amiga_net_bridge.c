/* SPDX-License-Identifier: GPL-3.0-only */
#if defined(__amigaos__) && !defined(DISABLE_NETWORK)
#include <proto/exec.h>
void* os_openlib(const char* name,long version) { return OpenLibrary((STRPTR)name,version); }
void os_closelib(void* base) { CloseLibrary(base); }
#endif
