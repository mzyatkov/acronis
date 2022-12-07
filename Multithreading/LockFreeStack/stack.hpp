#pragma once
#include <mutex>
#include <memory>


template <class T>
class stack
{
	struct node {
		node(T val, node* ptr = nullptr) : data(val), next(ptr) {}
		T data;
		node* next;
	};
public:
	stack() : head(nullptr) {}
	~stack() {}
	bool empty() const {
		return head != nullptr;
	}
	size_t size() const {
		size_t n = 0;
		for (node* p = head; p; p = p->next) ++n;
		return n;
	}
	void push(T val) {
		std::lock_guard<std::mutex> lock(mtx);
		node* p = new node(val, head);
		std::swap(p, head);
	}
	T pop() {
		std::lock_guard<std::mutex> lock(mtx);
		if (empty()) return 0;
		node* p = head;
		std::swap(head, p->next);
		std::unique_ptr<node> u(p);
		return p->data;
	}
private:
	std::mutex mtx;
	node* head;
};