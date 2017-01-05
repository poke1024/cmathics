#include <thread>
#include <cassert>

// the following class is a C++ port of Dmitry Vyukov's concurrent combiner; the comments were
// largely copied from Dmitry Vyukov's original post. his original blog post can be found at:
// https://software.intel.com/en-us/blogs/2013/02/22/combineraggregator-synchronization-primitive

template<typename DataStructure>
class Concurrent {
public:
	struct Argument : public DataStructure::Argument {
		std::atomic<Argument*> next;
	};

private:
	struct AsynchronousArgument : public Argument {
		Concurrent *concurrent = nullptr;

		AsynchronousArgument() {
			this->next.store(nullptr, std::memory_order_release);
		}

		~AsynchronousArgument() {
			if (concurrent) {
				// must not delete this while there is still a combiner working on it.
				concurrent->wait_for_argument(this);
			}
		}
	};

	class AsynchronousArguments {
	private:
		static constexpr int N = 1;

		uint8_t m_index;
		AsynchronousArgument m_arguments[N];

	public:
		AsynchronousArguments() : m_index(0) {
		}

		AsynchronousArgument *fetch() {
			AsynchronousArgument *argument = &m_arguments[(m_index++) % N];

			Argument *next = argument->next.load(std::memory_order_acquire);
			if (next == nullptr) {
				return argument;
			} else {
				return nullptr;
			}
		}
	};

	static constexpr uintptr_t locked = 1;
	static constexpr uintptr_t handoff = 2;
	static constexpr int limit = 16;

	inline bool is_locked(Argument *node) const {
		return uintptr_t(node) == locked;
	}

	inline bool test_handoff(Argument *node) const {
		return uintptr_t(node) & handoff;
	}

	inline Argument *set_handoff(Argument *node) const {
		return (Argument*)(uintptr_t(node) | handoff);
	}

	inline Argument *clear_handoff(Argument *node) const {
		return (Argument*)(uintptr_t(node) & ~handoff);
	}

	DataStructure m_data;
	std::atomic<Argument*> m_head;

	void wait_for_argument(Argument *argument) {
		size_t index = 0;

		while (true) {
			Argument *next = argument->next.load(std::memory_order_acquire);
			if (next == nullptr) {
				break;
			}

			// If we notice that our next pointer is marked with HANDOFF bit,
			// we have become the combiner.
			if (test_handoff(next)) {
				// Reset the HANDOFF bit to get the correct pointer.
				argument->next.store(clear_handoff(next), std::memory_order_release);
				int count = 0;
				combine(argument, count);
				combine_all(count);
				return;
			}

			// we might be the last thread running, i.e. there might be no
			// combiner left to serve us.
			Argument *cmp = m_head.load(std::memory_order_acquire);
			if (cmp == nullptr) {
				Argument *xchg = (Argument*)locked;
				if (m_head.compare_exchange_strong(cmp, xchg, std::memory_order_acq_rel)) {
					combine_all(0);
				}
			}

			if (++index > 1) {
				std::this_thread::yield();
			}
		}
	}

	inline bool combine(Argument *node, int &count) {
		assert(!test_handoff(node));

		// Execute the list of operations.
		while (!is_locked(node)) {
			Argument * const next = node->next.load(std::memory_order_acquire);

			// If we’ve reached the limit,
			// mark the current node with HANDOFF bit and return.
			// Owner of the node will execute the rest.
			if (count == limit) {
				node->next.store(set_handoff(next), std::memory_order_release);
				return false;
			}

			m_data(*node);
			count++;

			// Mark completion.
			node->next.store(nullptr, std::memory_order_release);

			node = next;
		}

		return true;
	}

	inline void combine_all(const int count0) {
		int count = count0;
		while (true) {
			Argument *cmp = m_head.load(std::memory_order_acquire);
			while (true) {
				// If there are some operations in the list,
				// grab the list and replace with LOCKED.
				// Otherwise, exchange to nullptr.

				Argument *xchg = nullptr;
				if (!is_locked(cmp)) {
					xchg = (Argument*)locked;
				}
				if (m_head.compare_exchange_strong(cmp, xchg, std::memory_order_acq_rel)) {
					break;
				}
			}
			// No more operations to combine, return.
			if (is_locked(cmp)) {
				break;
			}

			if (!combine(cmp, count)) {
				break;
			}
		}
	}

	static thread_local AsynchronousArguments s_asynchronous;

	template<bool Asynchronous>
	void serve(Argument *argument) {
		// 1. If m_head == 0, exchange it to LOCKED and become the combiner.
		// Otherwise, enqueue own node into the m_head lock­free list.

		Argument *cmp = m_head.load(std::memory_order_acquire);

		while (true) {
			Argument *xchg = (Argument*)locked;
			if (cmp) {
				// There is already a combiner, enqueue itself.
				xchg = argument;
				argument->next.store(cmp, std::memory_order_release);
			}

			if (m_head.compare_exchange_strong(cmp, xchg, std::memory_order_acq_rel)) {
				break;
			}
		}

		if (cmp) {
			// 2. If we are not the combiner, wait for arg­>next to become nullptr
			// (which means the operation is finished).

			if (Asynchronous) {
				return;
			}

			wait_for_argument(argument);

			// we got node.next through std::memory_order_acquire from a combiner who
			// stored it using std::memory_order_release, so we can be safe that our
			// "argument" is fully available.
		} else {
			// 3. We are the combiner.

			// First, execute own operation.
			m_data(*argument);

			// Mark as released.
			argument->next.store(nullptr, std::memory_order_release);

			// Then, look for combining opportunities.
			combine_all(1);
		}
	}

public:
	/*const DataStructure &data() const {
		// this is only safe to call if all concurrency has finished and only
		// one thread is running. in other words: only use this for debugging.
		return m_data;
	}*/

	Concurrent() {
		m_head.store(nullptr, std::memory_order_release);
	}

	template<typename F>
	void asynchronous(const F &f) {
		AsynchronousArgument *arg = s_asynchronous.fetch();

		if (arg) {
			arg->concurrent = this;
			f(*arg);
			serve<true>(arg);
		} else {
			Argument argument;
			f(argument);
			serve<false>(&argument);
		}
	}

	void operator()(Argument &argument) {
		serve<false>(&argument);
	}
};

template<typename DataStructure>
thread_local typename Concurrent<DataStructure>::AsynchronousArguments Concurrent<DataStructure>::s_asynchronous;


/*

#include <iostream>

 class ExampleData {
private:
	int m_count;

public:
	ExampleData() : m_count(0) {
	}

	void operator()(int &x) {
		m_count += x;
	}

	inline int get() const {
		return m_count;
	}
};

int counter = 0;

void f(Combiner<ExampleData, int> *combiner) {
	for (int i = 0; i < 1000000; i++) {
		int n = 1;
		(*combiner)(n);
		//counter++;
	}
}

int main() {
	Combiner<ExampleData, int> combiner;

	std::thread t1(f, &combiner);
	std::thread t2(f, &combiner);
	t1.join();
	t2.join();

	std::cout << combiner.data().get() << std::endl;

	return 0;
}
*/