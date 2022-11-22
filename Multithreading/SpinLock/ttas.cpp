#include "ttas.h"

spin_lock_TTAS::spin_lock_TTAS() {}
spin_lock_TTAS::~spin_lock_TTAS() { assert(m_spin.load(memory_order_acquire) == 0); }
string spin_lock_TTAS::name() { return "Spin lock TTAS                       | "; }
void spin_lock_TTAS::lock()
{
    unsigned int expected;
    do
    {
        while (m_spin.load(memory_order_acquire))
            ;
        expected = 0;
    } while (!m_spin.compare_exchange_weak(expected, 1, memory_order_acq_rel));
}
void spin_lock_TTAS::unlock()
{
    m_spin.store(0, memory_order_release);
}

yield_spin_lock_TTAS::yield_spin_lock_TTAS() {}
yield_spin_lock_TTAS::~yield_spin_lock_TTAS() { assert(m_spin.load(memory_order_acquire) == 0); }
string yield_spin_lock_TTAS::name() { return "Spin lock TTAS with yield            | "; }
void yield_spin_lock_TTAS::lock()
{
    unsigned int expected = 0;
    do
    {
        while (m_spin.load(memory_order_acquire))
        {
            _mm_pause();
            this_thread::yield();
        }
        expected = 0;
    } while (!m_spin.compare_exchange_weak(expected, 1, memory_order_acq_rel));
}
void yield_spin_lock_TTAS::unlock()
{
    m_spin.store(0, memory_order_release);
}

exp_bo_spin_lock_TTAS::exp_bo_spin_lock_TTAS() {}
exp_bo_spin_lock_TTAS::~exp_bo_spin_lock_TTAS() { assert(m_spin.load(memory_order_acquire) == 0); }
string exp_bo_spin_lock_TTAS::name() { return "Spin lock TTAS with exp back off     | "; }
void exp_bo_spin_lock_TTAS::lock()
{
    unsigned int expected = 0;
    // size_t max_power = 15;
    // double max_time = 0.01*pow(2, max_power);
    double max_time = 6553;
    size_t power = 0;
    do
    {
        while (m_spin.load(memory_order_acquire))
        {
            _mm_pause();
            int time = min(0.01 * pow(2, power) + rand() % 50, max_time + rand() % 50);
            power++;
            this_thread::sleep_for(chrono::microseconds(time));
        }
        expected = 0;
    } while (!m_spin.compare_exchange_weak(expected, 1));
}
void exp_bo_spin_lock_TTAS::unlock()
{
    m_spin.store(0, memory_order_release);
}
