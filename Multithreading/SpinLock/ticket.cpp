#include "ticket.h"

string ticket_lock::name() { return "ticket lock                            | "; }
void ticket_lock::lock()
{
    const auto ticket = next_ticket.fetch_add(1, memory_order_relaxed);
    while (now_serving.load(memory_order_acquire) != ticket);
}
void ticket_lock::unlock()
{
    const auto successor = now_serving.load(memory_order_relaxed) + 1;
    now_serving.store(successor, memory_order_release);
}

string yield_ticket_lock::name() { return "ticket lock with yield                 | "; }
void yield_ticket_lock::lock()
{
    const auto my_ticket = next_ticket.fetch_add(1, memory_order_relaxed);
    while (now_serving.load(memory_order_acquire) != my_ticket)
    {
        _mm_pause();
        this_thread::yield();
    }
}
void yield_ticket_lock::unlock()
{
    const auto successor = now_serving.load(memory_order_relaxed) + 1;
    now_serving.store(successor, memory_order_release);
}
string prop_bo_ticket_lock::name() { return "ticket lock with prop back off         | "; }
void prop_bo_ticket_lock::lock()
{
    const auto my_ticket = next_ticket.fetch_add(1, memory_order_relaxed);
    while (now_serving.load(memory_order_acquire) != my_ticket)
    {
        _mm_pause();
        const size_t now_between = my_ticket - now_serving;
        int time = 0.001 * now_between + rand() % 1000;
        this_thread::sleep_for(chrono::microseconds(time));
    }
}
void prop_bo_ticket_lock::unlock()
{
    const auto successor = now_serving.load(memory_order_relaxed) + 1;
    now_serving.store(successor, memory_order_release);
}
