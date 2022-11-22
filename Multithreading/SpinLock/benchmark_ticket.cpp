#include "ticket.h"

class Timer
{
public:
        Timer(string text) : text_(text)
        {
                start_ = std::chrono::steady_clock::now();
        }
        ~Timer()
        {
                auto end = std::chrono::steady_clock::now();
                auto dur = end - start_;
                cerr << text_ << std::chrono::duration_cast<std::chrono::milliseconds>(dur).count()
                     << " ms" << endl;
        }

private:
        string text_;
        std::chrono::time_point<std::chrono::steady_clock> start_;
};

template <typename T>
void Block_Function(T &waiting, const long long &count)
{
        waiting.lock();
        for (long long volatile i = 0; i < count; i++)
        {
        }
        waiting.unlock();
}

template <typename T>
void Benchmark(T &waiting, size_t counter, const long long &num_iterations)
{
        vector<thread> threads;
        {
                Timer timer(to_string(counter) + " threads | " + waiting.name() + " ");
                for (auto i = 0; i < counter; i++)
                {
                        threads.push_back(thread(Block_Function<T>, ref(waiting), cref(num_iterations)));
                }

                for (auto &th : threads)
                        if (th.joinable())
                                th.join();
        }
}

ticket_lock ticket;
prop_bo_ticket_lock ticket_prop;
yield_ticket_lock ticket_yield;

int main()
{
        long long count = 1000'000'0;
        size_t num_threads = 100;
        for (auto i = 10; i <= num_threads; i += 10)
        {
                Benchmark<ticket_lock>(ticket, i, count);
                Benchmark<prop_bo_ticket_lock>(ticket_prop, i, count);
                Benchmark<yield_ticket_lock>(ticket_yield, i, count);
        }
        return 0;
}