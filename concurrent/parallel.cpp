#include "parallel.h"
#include <cassert>
#include <iostream>

Parallel *Parallel::s_instance = nullptr;

constexpr int queue_size = 32;

inline Parallel::Spinlock::Spinlock(Parallel *parallel) : m_parallel(parallel), m_acquired(true) {
	while (parallel->m_lock.test_and_set(std::memory_order_acq_rel) == true) {
		// retry
	}
}

inline Parallel::Spinlock::~Spinlock() {
	clear();
}

inline void Parallel::Spinlock::clear() {
	if (m_acquired) {
		m_parallel->m_lock.clear(std::memory_order_release);
		m_acquired = false;
	}
}

thread_local std::mutex Parallel::Barrier::t_mutex;
thread_local std::condition_variable Parallel::Barrier::t_condition;

inline Parallel::Barrier::Barrier() :
	m_done(false),
	m_mutex(t_mutex),
	m_condition(t_condition),
	m_lock(t_mutex) {
}

inline void Parallel::Barrier::wait() {
	while (!m_done) {
		m_condition.wait(m_lock);
	}
}

inline void Parallel::Barrier::signal() {
	std::unique_lock<std::mutex> lock(m_mutex);
	m_done = true;
	m_condition.notify_all();
}


void Parallel::init() {
	s_instance = new Parallel();
}

void Parallel::shutdown() {
	delete s_instance;
}

Parallel::Parallel() : m_head(nullptr), m_size(0) {
	for (int i = 0L; i < std::max(1L, long(std::thread::hardware_concurrency()) - 1L); i++) {
		m_threads.push_back(std::make_unique<Thread>(this));
	}
}

Parallel::~Parallel() {
	while (m_threads.empty()) {
		m_threads.erase(m_threads.begin());
	}
}

bool Parallel::enqueue(Task *task) {
	assert(task->busy == 1);

	if (m_lock.test_and_set(std::memory_order_acq_rel) == true) {
		return false;
	}

	const size_t size = m_size;
	if (size >= queue_size) {
		m_lock.clear(std::memory_order_release);
		return false;
	}

	m_size = size + 1;

	Task * const head = m_head;

	task->next = head;
	task->prev = nullptr;
	task->enqueued = true;

	if (head) {
		head->prev = task;
	}

	m_head = task;

	m_lock.clear(std::memory_order_release);

	if (size == 0) {
		for (const auto &thread : m_threads) {
			thread->set_state(run);
		}
	}

	return true;
}

void Parallel::release(Task *task, bool owner) {
	Spinlock spinlock(this);

	if (task->enqueued) {
		if (task->prev) {
			task->prev->next = task->next;
		} else {
			m_head = task->next;
		}
		if (task->next) {
			task->next->prev = task->prev;
		}
		task->enqueued = false;
		m_size -= 1;
	}

	const int16_t busy = --task->busy;

	if (owner && busy > 0) {
		Barrier barrier;
		task->barrier = &barrier;
		spinlock.clear();
		barrier.wait();
	} else if (!owner && busy == 0) {
		const auto barrier = task->barrier;
		spinlock.clear();
		barrier->signal();
	} else {
		spinlock.clear();
	}
}

void Parallel::parallelize(const Lambda &lambda, size_t n) {
	Task task(lambda, n);
	bool enqueued = false;

	while (true) {
		if (!enqueued) {
			enqueued = enqueue(&task);
		}
		const size_t index = task.index.fetch_add(1, std::memory_order_relaxed);
		if (index >= n) {
			break;
		}
		lambda(index);
	}

	if (enqueued) {
		release(&task, true);
	}
}

Parallel::Thread::Thread(Parallel *parallel) :
	m_parallel(parallel),
	m_state(block),
	m_thread(&Parallel::Thread::work, this) {

}

Parallel::Thread::~Thread() {
	set_state(quit);
	m_thread.join();
}

void Parallel::Thread::set_state(ThreadState state)  {
	std::unique_lock<std::mutex> lock(m_mutex);
	m_state = state;
	m_event.notify_all();
}

void Parallel::Thread::work() {
	std::unique_lock<std::mutex> lock(m_mutex);

	Parallel * const parallel = m_parallel;

	while (true) {
		while (m_state == block) {
			m_event.wait(lock);
		}
		if (m_state == quit) {
			break;
		}

#if 0
		std::cout << "unblocking thread " << std::this_thread::get_id() << std::endl;
#endif

		// process until head == nullptr, i.e. each new add will
		// have to trigger a set_state(run) in Parallel::enqueue.

		while (true) {
			Task *head;

			{
				Spinlock spinlock(parallel);

				head = parallel->m_head;

				if (head == nullptr) {
					break;
				}

				head->busy += 1;
			}

			const size_t n = head->n;
			const auto &lambda = head->lambda;

			while (true) {
				const size_t index = head->index.fetch_add(1, std::memory_order_relaxed);
				if (index >= n) {
					break;
				}
				lambda(index);
			}

			parallel->release(head, false);
		}

		m_state = block;
	}
}
