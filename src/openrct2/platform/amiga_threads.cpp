/*****************************************************************************
 * Copyright (c) 2026 OpenRCT2 developers
 * SPDX-License-Identifier: GPL-3.0-only
 *****************************************************************************/
#ifdef __amigaos__
#include <pthread.h>

extern "C" int __wrap_pthread_create(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*);
extern "C" int __real_pthread_create(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*);

// std::thread supplies no attributes. Give its worker a deliberate stack instead
// of relying on the runtime default. Explicit caller attributes retain their meaning.
extern "C" int __wrap_pthread_create(
    pthread_t* thread, const pthread_attr_t* attributes, void* (*start)(void*), void* argument)
{
    if (attributes != nullptr)
        return __real_pthread_create(thread, attributes, start, argument);
    pthread_attr_t workerAttributes;
    int result = pthread_attr_init(&workerAttributes);
    if (result != 0)
        return result;
    result = pthread_attr_setstacksize(&workerAttributes, 256 * 1024);
    if (result == 0)
        result = __real_pthread_create(thread, &workerAttributes, start, argument);
    pthread_attr_destroy(&workerAttributes);
    return result;
}
#endif
