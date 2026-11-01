/* SPDX-License-Identifier: GPL-3.0-only */
#include <openrct2/core/JobPool.h>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <set>
int main()
{
    const auto mainThread=std::this_thread::get_id();
    {
        JobPool loadingPool(0); unsigned work=0, completed=0;
        for(unsigned i=0;i<128;++i)
            loadingPool.AddTask([&]{ assert(std::this_thread::get_id()==mainThread);++work; },
                [&]{ assert(std::this_thread::get_id()==mainThread);++completed; });
        loadingPool.Join();assert(work==128 && completed==128 && !loadingPool.IsBusy());
    }
    for (int round=0;round<8;++round)
    {
        JobPool pool(64);
        std::atomic<unsigned> work{0}; unsigned completed=0;
        std::set<std::thread::id> workers; std::mutex lock;
        for (unsigned i=0;i<128;++i)
            pool.AddTask([&] {
                assert(std::this_thread::get_id()!=mainThread);
                std::lock_guard guard(lock);workers.insert(std::this_thread::get_id());++work;
            },[&] { assert(std::this_thread::get_id()==mainThread);++completed; });
        pool.Join(); assert(work==128 && completed==128 && workers.size()>=1 && workers.size()<=2);
        for (unsigned i=0;i<32;++i) pool.AddTask([&]{++work;},[&]{++completed;});
        pool.Join();assert(work==160 && completed==160 && !pool.IsBusy());
    }
    puts("JobPool: explicit inline loading and actual worker execution, two-worker limit, main-task completion, reuse and repeated shutdown PASS");
}
