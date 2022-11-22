#pragma once

#include <iostream>
#include <mutex>
#include <thread>
#include <cmath>
#include <atomic>
#include "assert.h"
#include <typeinfo>
#include <immintrin.h>
#include <vector>
using namespace std;

class spin_lock_TTAS
{
    atomic<unsigned int> m_spin = 0;

public:
    spin_lock_TTAS();
    ~spin_lock_TTAS();
    string name();
    void lock();
    void unlock();
};
class yield_spin_lock_TTAS
{
    atomic<unsigned int> m_spin = 0;

public:
    yield_spin_lock_TTAS();
    ~yield_spin_lock_TTAS();
    string name();
    void lock();
    void unlock();
};

class exp_bo_spin_lock_TTAS
{
    atomic<unsigned int> m_spin = 0;

public:
    exp_bo_spin_lock_TTAS();
    ~exp_bo_spin_lock_TTAS();
    string name();
    void lock();
    void unlock();
};
