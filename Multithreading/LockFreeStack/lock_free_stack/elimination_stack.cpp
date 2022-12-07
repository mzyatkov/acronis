#include <thread>
#include <assert.h>
#include <atomic>
#include <thread>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <mutex>
#include <cmath>
#include <stack>
#include <iostream>
#include <chrono>
#include <fstream>

#define OP_NUM 10000
#define MAX_THREADS 100

#define SIMPLE 0
#define RANDOM 1
#define UNBALANCED 1

int thread_num = 0;

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
        int time = std::min(0.01 * pow(2, nCurrent) + rand() % 50, nThreshold + rand() % 50);
        nCurrent++;
        std::this_thread::sleep_for(std::chrono::microseconds(time));
    }
    void reset() { nCurrent = nInitial; }
};

class mutex_stack
{
public:
    void Push(int entry)
    {
        mutex.lock();
        simple_stack.push(entry);
        mutex.unlock();
    }

    int Pop()
    {
    	mutex.lock();
        if(simple_stack.empty())
        {
            return -1;
        }
        int ret = simple_stack.top();
        simple_stack.pop();
        mutex.unlock();
        return ret;
    }

private:
    std::stack<int> simple_stack;
    std::mutex mutex;
};
// class mutex_stack
// {
// 	struct node {
// 		node(int val, node* ptr = nullptr) : data(val), next(ptr) {}
// 		int data;
// 		node* next;
// 	};
// public:
// 	mutex_stack() : head(nullptr) {}
// 	~mutex_stack() {}
// 	bool empty() const {
// 		return head != nullptr;
// 	}
// 	size_t size() const {
// 		size_t n = 0;
// 		for (node* p = head; p; p = p->next) ++n;
// 		return n;
// 	}
// 	void Push(int val) {
// 		std::lock_guard<std::mutex> lock(mtx);
// 		node* p = new node(val, head);
// 		std::swap(p, head);
// 	}
// 	int Pop() {
// 		std::lock_guard<std::mutex> lock(mtx);
// 		if (empty()) return 0;
// 		node* p = head;
// 		std::swap(head, p->next);
// 		std::unique_ptr<node> u(p);
// 		return p->data;
// 	}
// private:
// 	std::mutex mtx;
// 	node* head;
// };

#define FREELIST_SIZE 40

class lock_free_stack {
public:

	struct Cell {
		std::atomic<Cell*> next;
	};

	class tagged_pointer {
	public:
		tagged_pointer() :
				cell(nullptr), counter(0) {
		}

		Cell* get_cell() {
			return cell.load(std::memory_order_acquire);
		}

		uint64_t get_counter() {
			return counter.load(std::memory_order_acquire);
		}

		bool compare_and_swap(Cell *oldNode, uint64_t oldCounter, Cell *newNode, uint64_t newCounter) {
			bool cas_result = 0;
			__asm__ __volatile__
			(
					"lock cmpxchg16b %0;"  // cmpxchg16b sets ZF on success
					"setz       %3;"// if ZF set, set cas_result to 1

					: "+m" (*this), "+a" (oldNode), "+d" (oldCounter), "=q" (cas_result)
					: "b" (newNode), "c" (newCounter)
					: "cc", "memory"
			);
			return cas_result;
		}

private:

		std::atomic<Cell*> cell;
		std::atomic<uint64_t> counter;
	}

	// у нас cas на 16 байт
	__attribute__((aligned(16)));

	bool TryPushStack(Cell *entry) {
		Cell *oldHead = NULL;
		uint64_t oldCounter = 0;

		oldHead = head.get_cell();
		oldCounter = head.get_counter();

		entry->next.store(oldHead, std::memory_order_relaxed);

		return head.compare_and_swap(oldHead, oldCounter, entry, oldCounter + 1);
	}

	bool TryPopStack(Cell *&oldHead, int threadId) {
		oldHead = head.get_cell();
		uint64_t oldCounter = head.get_counter();

		if (oldHead == nullptr) {
			return true;
		}

		hazards[threadId].store(oldHead, std::memory_order_seq_cst);

		if (head.get_cell() != oldHead) {
			return false;
		}

		return head.compare_and_swap(oldHead, oldCounter, oldHead->next.load(std::memory_order_acquire), oldCounter + 1);
	}

	void Push(int threadId, Cell *entry) {
		exp_backoff backoff;
		pushs[threadId].store(1);
		while (true) {
			if (TryEliminatePush(threadId)) {
				return;
			}
			if (TryPushStack(entry)) {
				pushs[threadId].store(0);
				return;
			}
			backoff();
		}
	}

	bool canBeFreed(Cell * nd) {
		for (int i = 0; i < thread_num; i++) {
			if (hazards[i].compare_exchange_weak(nd, nd, std::memory_order_relaxed)) {
				return 0;
			}
		}
		return 1;
	}

	void addToFreeList(Cell * nd) {
		for (int i = 0; i < FREELIST_SIZE; i++) {
			Cell * tmp = nullptr;
			if (freelist[i].compare_exchange_weak(tmp, nd, std::memory_order_relaxed)) {
				return;
			}
		}

		for (int i = 0; i < FREELIST_SIZE; i++) {
			Cell * val = freelist[i].exchange(nullptr, std::memory_order_relaxed);
			if (val != nullptr && canBeFreed(val)) {
				delete val;
			}
		}
	}


	bool TryEliminatePop(int threadId) {
		bool v = 0;
		if (pops[threadId].compare_exchange_weak(v, v)) {
			return 1;
		}
		for (int i = 0; i < thread_num; i++) {
			v = 1;
			if (pushs[threadId].compare_exchange_weak(v, 0)) {
				pops[threadId].store(0);
				return 1;
			}
		}
		return 0;
	}

	bool TryEliminatePush(int threadId) {
		bool v = 0;
		if (pushs[threadId].compare_exchange_weak(v, v)) {
			return 1;
		}
		for (int i = 0; i < thread_num; i++) {
			v = 1;
			if (pops[threadId].compare_exchange_weak(v, 0)) {
				pushs[threadId].store(0);
				return 1;
			}
		}
		return 0;
	}

	void Pop(int threadId) {
		exp_backoff backoff;
		Cell *res = NULL;
		pops[threadId].store(1);
		while (true) {
			if (TryEliminatePop(threadId)) {
				return;
			}
			if (TryPopStack(res, threadId)) {
				pops[threadId].store(0);
				hazards[threadId].store(nullptr);
				if (canBeFreed(res)) {
					delete res;
				} else {
					addToFreeList(res);
				}
				return;
			}
			backoff();
		}
	}

private:

	tagged_pointer head;
	std::atomic<Cell*> hazards[MAX_THREADS] = {};
	std::atomic<Cell*> freelist[FREELIST_SIZE];

	std::atomic<bool> pops[MAX_THREADS] = {};
	std::atomic<bool> pushs[MAX_THREADS] = {};
};


void work_lockfree(lock_free_stack * st, int index) {
	for (int i = 0; i < OP_NUM; i++) {
		#if SIMPLE
		if (i%2==0) {
		#elif RANDOM
		if (rand()%2==0) {
		#elif UNBALANCED
		if ((index + (int)(i / 1000)) % 2 == 0) {
		#endif
			st->Push(index, new lock_free_stack::Cell);
			break;
		} else {
			st->Pop(index);
			break;
		}
	}
}

void work_locked(mutex_stack * st, int index) {

	for (int i = 0; i < OP_NUM; i++) {
		#if SIMPLE
		if (i%2==0) {
		#elif RANDOM
		if (rand()%2==0) {
		#elif UNBALANCED 
		if ((index + (int)(i / 1000)) % 2 == 0) {
		#endif
			st->Push(rand());
			break;
		} else {
			st->Pop();
			break;
		}
	}
}

void run_lockfree(int numThreads) {
	assert(numThreads < MAX_THREADS);

	std::thread *threads[MAX_THREADS] = { 0 };

	lock_free_stack *st = new lock_free_stack();

	for (int i = 0; i < numThreads; i++) {
		threads[i] = new std::thread(work_lockfree, st, i);
	}

	for (int i = 0; i < numThreads; i++) {
		threads[i]->join();
	}

	for (int i = 0; i < numThreads; i++) {
		delete threads[i];
	}
}

void run_locked(int numThreads) {
	assert(numThreads < MAX_THREADS);

	std::thread *threads[MAX_THREADS] = { 0 };

	mutex_stack *st = new mutex_stack();

	for (int i = 0; i < numThreads; i++) {
		threads[i] = new std::thread(work_locked, st, i);
	}

	for (int i = 0; i < numThreads; i++) {
		threads[i]->join();
	}

	for (int i = 0; i < numThreads; i++) {
		delete threads[i];
	}
}

long time_lockfree(int numThreads) {
	auto time_begin = std::chrono::high_resolution_clock::now();
	run_lockfree(numThreads);
	auto time_end = std::chrono::high_resolution_clock::now();

	auto dtime = time_end - time_begin;
	long dtime_ms = std::chrono::duration_cast<std::chrono::microseconds>(dtime).count();
	return dtime_ms;
}

long time_locked(int numThreads) {
	auto time_begin = std::chrono::high_resolution_clock::now();
	run_locked(numThreads);
	auto time_end = std::chrono::high_resolution_clock::now();

	auto dtime = time_end - time_begin;
	long dtime_ms = std::chrono::duration_cast<std::chrono::microseconds>(dtime).count();
	return dtime_ms;
}

void test_lockfree_stack() {
	for (int i = 1; i < MAX_THREADS; i++) {
		thread_num = i;
		long dt = time_lockfree(i);
		std::cout << i << "," << dt << std::endl;
	}
}

void test_locked_stack() {
	for (int i = 1; i < MAX_THREADS; i++) {
		thread_num = i;
		long dt = time_locked(i);
		std::cout << i << "," << dt << std::endl;
	}
}

int main(int argc, char *argv[]) {
	std::ofstream out("random_locked.csv"); //откроем файл для вывод
	std::cout.rdbuf(out.rdbuf()); //и теперь все будет в файл
	// test_lockfree_stack();
	test_locked_stack();
	return 0;
}