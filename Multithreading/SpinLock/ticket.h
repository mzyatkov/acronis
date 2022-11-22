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

class ticket_lock
{
    atomic<unsigned int> now_serving = {0};
    atomic<unsigned int> next_ticket = {0};

public:
    string name();
    void lock();
    void unlock();
};

class yield_ticket_lock
{
    atomic<unsigned int> now_serving = {0};
    atomic<unsigned int> next_ticket = {0};

public:
    string name();
    void lock();
    void unlock();
};

class prop_bo_ticket_lock
{
    atomic<unsigned int> now_serving = {0};
    atomic<unsigned int> next_ticket = {0};

public:
    string name();
    void lock();
    void unlock();
};