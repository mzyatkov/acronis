#include "stack.hpp"
#include "elimination.hpp"
#include <thread>
#include <vector>
#include <iostream>

typedef stack<int> sstack_t;

void worker(sstack_t& s, size_t nitems)
{
	for (size_t i = 0; i < nitems; ++i) {
        if (i % 2 == 0) {
            s.push(i);
        } else {
            s.pop();
        }   

    }
}

void work(sstack_t& s, size_t nitems, size_t nthreads)
{
	std::vector<std::thread> w;
	w.reserve(nthreads);
	for (size_t i = 0; i != nthreads; ++i)
		w.emplace_back(worker, std::ref(s), nitems);
	for (size_t i = 0; i != w.size(); ++i)
		w[i].join();
}
void elim_worker(elimination_stack& s, size_t nitems, int thread)
{
	for (size_t i = 0; i < nitems; ++i) {
        if (rand() % 2 == 0) {
            s.push(nullptr, thread);
        } else {
            s.pop(thread);
        }   

    }
}

void elim_work(elimination_stack& s, size_t nitems, size_t nthreads)
{
	std::vector<std::thread> w;
	w.reserve(nthreads);
	for (size_t i = 0; i != nthreads; ++i)
		w.emplace_back(elim_worker, std::ref(s), nitems, i);
	for (size_t i = 0; i != w.size(); ++i)
		w[i].join();
}
int main(int argc, char* argv[])
{
	size_t nitems   = argc > 1 ? atoi(argv[1]) : 10000;
	size_t nthreads = argc > 2 ? atoi(argv[2]) : THREAD_NUM;
    elimination_stack es; 
    elim_work(es, nitems, nthreads);
}