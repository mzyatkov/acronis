#include "ttas.h"
#include "ticket.h"
#include <numeric>
#include <algorithm>

#define ITER_COUNT 1000'000'0
#define TTAS_TEST  0
#define TICKET_TEST 1
#if TTAS_TEST
spin_lock_TTAS TTAS;
exp_bo_spin_lock_TTAS TTAS_exp;
yield_spin_lock_TTAS TTAS_yield;
#elif TICKET_TEST
ticket_lock ticket;
prop_bo_ticket_lock ticket_prop;
yield_ticket_lock ticket_yield;
#endif

template <typename T>
void Block_Function(T &waiting, long int &time)
{
    auto start = std::chrono::steady_clock::now();
    waiting.lock();
    auto end = std::chrono::steady_clock::now();
    time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    for (long long volatile i = 0; i < ITER_COUNT; i++)
    {
    }
    waiting.unlock();
}

template <typename T>
void Benchmark(T &waiting, size_t counter)
{
    vector<thread> threads;
    vector<long int> times(counter);
    {
        for (auto i = 0; i < counter; i++)
        {
            threads.push_back(thread(Block_Function<T>, ref(waiting), ref(times[i])));
        }

        for (auto &th : threads)
            if (th.joinable())
                th.join();
    }
    // cerr << to_string(counter) + " threads | " + waiting.name() << "max: " << *std::max_element(times.begin(), times.end()) << " ms, "
        //  << "avg: " << std::accumulate(times.begin(), times.end(), 0) / times.size() << " ms" << endl;
    cerr << *std::max_element(times.begin(), times.end())  << ',' << std::accumulate(times.begin(), times.end(), 0) / times.size()<< endl; 
}


int main()
{

#if TTAS_TEST
    size_t num_threads = 100;
    for (auto i = 1; i <= num_threads; i += 2)
    {
        Benchmark<spin_lock_TTAS>(TTAS, i);
        Benchmark<exp_bo_spin_lock_TTAS>(TTAS_exp, i);
        Benchmark<yield_spin_lock_TTAS>(TTAS_yield, i);
    }

#elif TICKET_TEST
    size_t num_threads = 100;
    for (auto i = 1; i <= num_threads; i += 2)
    {
        Benchmark<ticket_lock>(ticket, i);
        Benchmark<prop_bo_ticket_lock>(ticket_prop, i);
        Benchmark<yield_ticket_lock>(ticket_yield, i);
    }
#endif
    return 0;
}