#include <iostream>
#include <mutex>
#include <thread>
#include <cmath>
#include <atomic>
#include "assert.h"
#include <typeinfo>
#include <immintrin.h>
#include <vector>
#include <algorithm>

using namespace std;

#define MAX_FACTOR 1.
#define MIN_FACTOR 0.
#define ADAPT_INIT 3
#define MAX_COUNT 10
#define THREAD_NUM 100
#define PUSH 1
#define POP 2
#define  EMPTY nullptr

struct Cell
{
    Cell *pnext;
    void *pdata;
};
struct Simple_Stack
{
    atomic<Cell *> ptop;
};
struct AdaptParams
{
    int count;
    double factor;
};
struct ThreadInfo
{
    unsigned int id;
    char op;
    Cell *cell;
    AdaptParams *adapt;
};
enum direction
{
    SHRINK,
    ENLARGE
};


class exp_backoff
{
    int const nInitial;
    int const nStep;
    double const nThreshold;
    int nCurrent;

public:
    exp_backoff(int init = 0, int step = 1, double threshold = 8000)
        : nInitial(init), nStep(step), nThreshold(threshold), nCurrent(init)
    {
    }
    void operator()()
    {
        int time = min(0.01 * pow(2, nCurrent) + rand() % 50, nThreshold + rand() % 50);
        nCurrent++;
        this_thread::sleep_for(chrono::microseconds(time));
    }
    void reset() { nCurrent = nInitial; }
};

class elimination_stack
{
public:
    Simple_Stack S;
    atomic<ThreadInfo*> *location;
    atomic<int> *collision;

    elimination_stack()
    {
        S.ptop = EMPTY;
        location = new atomic<ThreadInfo *> [THREAD_NUM];
        collision = new atomic<int>[MAX_COUNT];
        for (int i = 0; i < MAX_COUNT; i++)
        {
            location[i] = EMPTY;
            collision[i] = 0;
        }
    }
    ~elimination_stack()
    {
        for (int i = 0; i < MAX_COUNT; i++)
        {
            if (location[i] != EMPTY)
            {
                delete location[i].load();
            }
        }
        delete[] location;
        delete[] collision;
    }
    void push(void *data, int mypid)
    {
        ThreadInfo *p = new ThreadInfo;
        p->op = PUSH;
        p->cell = new Cell;
        p->cell->pdata = data;
        p->cell->pnext = EMPTY;
        p->id = mypid;
        p->adapt = new AdaptParams;
        p->adapt->count = ADAPT_INIT;
        p->adapt->factor = 0.5;
        StackOp(p);
        delete p->adapt;
        delete p;
    }
    void *pop(int mypid)
    {
        ThreadInfo *p = new ThreadInfo;
        p->op = POP;
        p->cell = EMPTY;
        p->id = mypid;
        p->adapt = new AdaptParams;
        p->adapt->count = ADAPT_INIT;
        p->adapt->factor = 0.5;
        StackOp(p);
        if (p->cell == EMPTY)
        {
            delete p->adapt;
            delete p;
            return EMPTY;
        }
        void *data = p->cell->pdata;
        delete p->adapt;
        delete p;
        return data;
    }
    int size()
    {
        int size = 0;
        Cell *p = S.ptop;
        while (p != EMPTY)
        {
            size++;
            p = p->pnext;
        }
        return size;
    }

private:
    int him;
    void StackOp(ThreadInfo *p)
    {
        if (TryPerformStackOp(p) == false)
        {
            LesOP(p);
        }
        return;
    }
    int GetPosition(ThreadInfo *p)
    {
        int size = MAX_COUNT * p->adapt->factor;
        srand(time(NULL));
        int pos = rand() % size;
        return pos;
    }

    void LesOP(ThreadInfo *p)
    {
        exp_backoff backoff;
        while (true)
        {
            int mypid  = p->id;
            location[mypid] = p;
            int pos = GetPosition(p);
            him = collision[pos].load();
            while (!collision[pos].compare_exchange_weak(him, mypid))
            {
                him = collision[pos];
            }
            if (him != -1)
            {
                ThreadInfo *q = location[him];
                if (q != nullptr and q->id == him and q->op != p->op)
                {
                    if (location[him].compare_exchange_weak(q, nullptr))
                    {
                        if (TryCollision(p, q, mypid) == true)
                        {
                            return;
                        }
                        else
                        {
                            if (TryPerformStackOp(p) == true)
                            {
                                return;
                            }
                        }
                    }
                    else
                    {
                        FinishCollision(p, mypid);
                        return;
                    }
                }
            }
            backoff();
            AdaptWidth(SHRINK, p);
            if (!location[mypid].compare_exchange_weak(p, nullptr))
            {
                FinishCollision(p, mypid);
                return;
            }
            if (TryPerformStackOp(p) == true)
            {
                return;
            }
        }
    }
    bool TryPerformStackOp(ThreadInfo *p)
    {
        Cell *ptop = EMPTY;
        Cell *pnext = EMPTY;
        if (p->op == PUSH)
        {
            ptop = S.ptop;
            p->cell->pnext = ptop;
            if (S.ptop.compare_exchange_weak(ptop, p->cell))
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        if (p->op == POP)
        {
            ptop = S.ptop;
            if (ptop == nullptr)
            {
                p->cell = EMPTY;
                return true;
            }
            pnext = ptop->pnext;
            if (S.ptop.compare_exchange_weak(ptop, pnext))
            {
                p->cell = ptop;
                return true;
            }
        }
        return false;
    }

    bool TryCollision(ThreadInfo *p, ThreadInfo *q, int mypid)
    {
        if (p->op == PUSH)
        {
            if (location[him].compare_exchange_weak(q, p))
                return true;
            else
            {
                AdaptWidth(ENLARGE, p);
                return false;
            }
        }
        if (p->op == POP)
        {
            if (location[him].compare_exchange_weak(q, NULL))
            {
                p->cell = q->cell;
                location[mypid] = NULL;
                return true;
            }
            else
            {
                AdaptWidth(ENLARGE, p);
                return false;
            }
        }
        return false;
    }
    void FinishCollision(ThreadInfo *p, int mypid)
    {
        if (p->op == POP)
        {
            p->cell = location[mypid].load()->cell;
            location[mypid] = NULL;
        }
    }

    void AdaptWidth(direction direction, ThreadInfo *p)
    {
        if (direction == SHRINK)
        {
            if (p->adapt->count > 0)
            {
                p->adapt->count--;
            }
            else
            {
                p->adapt->count = ADAPT_INIT;
                p->adapt->factor = max(p->adapt->factor / 2, MIN_FACTOR);
            }
        }
        else if (p->adapt->count < MAX_COUNT)
        {
            p->adapt->count++;
        }
        else
        {
            p->adapt->count = ADAPT_INIT;
            p->adapt->factor = min(2 * p->adapt->factor, MAX_FACTOR);
        }
    }
};