/* SPDX-License-Identifier: GPL-3.0-only */
// Compile the real JobPool under its Amiga branches on the host. This checks
// dispatch/completion semantics; it does not claim AmigaOS runtime coverage.
#include "../../../src/openrct2/core/JobPool.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <mutex>
#include <set>

int main()
{
    const auto caller = std::this_thread::get_id();
    for (int run = 0; run < 20; ++run)
    {
        JobPool pool(255);
        std::atomic<int> total{ 0 };
        int callbacks = 0;
        std::mutex mutex;
        std::set<std::thread::id> executors;
        for (int i = 1; i <= 128; ++i)
            pool.AddTask([&, i] {
                total += i;
                std::lock_guard lock(mutex);
                executors.insert(std::this_thread::get_id());
            }, [&] {
                assert(std::this_thread::get_id() == caller);
                ++callbacks;
            });
        pool.Join();
        assert(total == 8256 && callbacks == 128 && !pool.IsBusy());
#ifdef OPENRCT2_AMIGA_THREADS
        assert(!executors.empty() && executors.size() <= 2 && !executors.contains(caller));
#else
        assert(executors.size() == 1 && executors.contains(caller));
#endif
        // Reuse after Join and verify null completion functions are accepted.
        pool.AddTask([&] { ++total; });
        pool.Join();
        assert(total == 8257);
    }
    std::puts("JobPool work/completion/reuse/shutdown: PASS");
}
