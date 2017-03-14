#include "../core/runtime.h"
#include "parallel.h"
#include <cassert>
#include <iostream>

Parallel *Parallel::s_instance = nullptr;

thread_local ParallelContext Parallel::t_context;

constexpr int queue_size = 32;

// FORCE_SEQUENTIAL_EXECUTION makes all parallelize calls
// run in one thread thereby disabling parallelization.
// useful for debugging.

#define FORCE_SEQUENTIAL_EXECUTION 0

bool Version::equivalent_to(const Version *version) const {
	const Version *compare = this;

	do {
		if (version == compare) {
			return true;
		}
		compare = compare->master().get();
	} while (compare);

	return false;
}

using UnsafeVersionRef = UnsafeSharedPtr<Version>;

/*struct ConcurrentDefinitions {
	UnsafeVersionRef version;
	SymbolStateMap states;
};*/

/*inline const SymbolState *ParallelTask::symbol_state(
	const Parallel::Context &context, const Symbol *symbol) const {

	const ConcurrentDefinitions * const def = definitions[context.thread_number];
	if (!def) {
		return &symbol->m_user_state;
	}

	const auto &m = def->states;
	const auto i = m.find(symbol);
	if (i != m.end()) {
		return &i->second;
	} else if (context.parent) {
		return context.parent->task->symbol_state(*context.parent, symbol);
	}

	return &symbol->m_user_state;
}

inline SymbolState *ParallelTask::mutable_symbol_state(
	const Parallel::Context &context, const Symbol *symbol) {

	ConcurrentDefinitions *&def = definitions[context.thread_number];
	if (!def) {
		def = new ConcurrentDefinitions;
	}

	// def->update_version(base_version);
	auto &m = def->states;
	const auto i = m.find(symbol);
	if (i != m.end()) {
		return &i->second;
	} else if (context.parent) {
		const SymbolState *base =
			context.parent->task->symbol_state(*context.parent, symbol);
		return &m.emplace(SymbolRef(symbol), *base).first->second;
	}

	return &symbol->m_user_state;
}

inline VersionRef ParallelTask::definitions_version(const Parallel::Context &context) const {
	const ConcurrentDefinitions * const def = definitions[context.thread_number];
	if (def) {
		return def->version;
	} else {
		return base_version;
	}
}

inline void ParallelTask::update_definitions_version(const Parallel::Context &context) {
	ConcurrentDefinitions *&def = definitions[context.thread_number];
	if (!def) {
		def = new ConcurrentDefinitions;
	}
	def->version = Version::construct(base_version);
}*/

inline Parallel::Spinlock::Spinlock(Parallel *parallel) :
    m_parallel(parallel), m_acquired(false) {

    acquire();
}

inline Parallel::Spinlock::~Spinlock() {
	release();
}

inline void Parallel::Spinlock::acquire() {
    if (!m_acquired) {
        while (m_parallel->m_lock.test_and_set(std::memory_order_acq_rel) == true) {
            // retry
        }
        m_acquired = true;
    }
}

inline void Parallel::Spinlock::release() {
	if (m_acquired) {
		m_parallel->m_lock.clear(std::memory_order_release);
		m_acquired = false;
	}
}

thread_local std::mutex ParallelBarrier::t_mutex;
thread_local std::condition_variable ParallelBarrier::t_condition;

inline ParallelBarrier::ParallelBarrier() :
	m_done(false),
	m_mutex(t_mutex),
	m_condition(t_condition),
	m_lock(t_mutex) {
}

inline void ParallelBarrier::wait() {
	while (!m_done) {
		m_condition.wait(m_lock);
	}
}

inline void ParallelBarrier::signal() {
	std::unique_lock<std::mutex> lock(m_mutex);
	m_done = true;
	m_condition.notify_all();
}


void Parallel::init() {
	s_instance = new Parallel();
	// we're now in the main thread, so initialize its context.
	t_context.thread_number = 0;
	t_context.task = nullptr;
	t_context.parent = nullptr;
}

void Parallel::shutdown() {
	delete s_instance;
}

Parallel::Parallel() : m_head(nullptr), m_size(0) {
	const size_t n = std::min(
		long(MaxParallelism - 1),
  	    std::max(1L, long(std::thread::hardware_concurrency()) - 1L));

	for (int i = 0L; i < n; i++) {
		m_threads.push_back(std::make_unique<Thread>(this, i + 1));
	}
}

Parallel::~Parallel() {
	while (m_threads.empty()) {
		m_threads.erase(m_threads.begin());
	}
}

bool Parallel::enqueue(ParallelTask *task) {
	assert(task->busy == 1);

    if (m_size.load(std::memory_order_relaxed) >= queue_size) {
        return false;
    }

	if (m_lock.test_and_set(std::memory_order_acq_rel) == true) {
		return false;
	}

	const size_t size = m_size.load(std::memory_order_relaxed);
	if (size >= queue_size) {
		m_lock.clear(std::memory_order_release);
		return false;
	}

    m_size.store(size + 1, std::memory_order_relaxed);

	ParallelTask * const head = m_head;

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
            if (thread->get_id() != std::this_thread::get_id()) {
                thread->set_state(run);
            }
		}
	}

	return true;
}

void Parallel::release(ParallelTask *task, bool owner) {
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
		m_size.fetch_add(-1, std::memory_order_relaxed);
	}

	const int16_t busy = --task->busy;

	if (owner && busy > 0) {
        task->barrier = nullptr;
        spinlock.release();

		ParallelBarrier barrier;
		spinlock.acquire();
        if (task->busy > 0) {
            task->barrier = &barrier;
            spinlock.release();

            barrier.wait();
        }
	} else if (!owner && busy == 0) {
		ParallelBarrier * const barrier = task->barrier;
		spinlock.release();
        if (barrier != nullptr) {
            barrier->signal();
        }
	} else {
		spinlock.release();
	}
}

void Parallel::parallelize(const ParallelTask::Lambda &lambda, size_t n, const Evaluation &evaluation) {
	// calls lambda(i) for i = 0, ..., n - 1. if other threads are free,
	// work is split among them. if no other threads are free, this is
	// just a sequential execution. we check if a thread became free after
	// each iteration (i.e. each call of lambda()).

	// note that lambda must be thread safe in any case, as it might be
	// called by multiple worker threads at the same time.

	ParallelContext &context = t_context;
	ParallelContext parent = context;

	ParallelTask task(lambda, n, evaluation.definitions.version(), evaluation);
	bool enqueued = false;

	context.task = &task;
	context.parent = parent.task ? &parent : nullptr;

	try {
		while (true) {
#if !FORCE_SEQUENTIAL_EXECUTION
			if (!enqueued) {
				enqueued = enqueue(&task);
			}
#endif

			const size_t index = task.index.fetch_add(
				1, std::memory_order_relaxed);

			if (index >= n) {
				break;
			}
			lambda(index);
		}
	} catch(...) {
		std::cerr << "Parallel::parallelize: uncaught error";
	}

	context = parent;

	if (enqueued) {
		release(&task, true);
	} else {
		--task.busy;
	}

	assert(!task.enqueued);
	assert(task.busy == 0);
}

Parallel::Thread::Thread(Parallel *parallel, ThreadNumber thread_number) :
	m_parallel(parallel),
	m_state(block),
	m_thread_number(thread_number),
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
    // this runs in a thread and steals work from other
    // threads by inspecting the queue (m_head) and grabbing
    // work items.

	ParallelContext &context = t_context;

	context.thread_number = m_thread_number;
	context.task = nullptr;
	context.parent = nullptr;

	Parallel * const parallel = m_parallel;

	while (true) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            while (m_state == block) {
                m_event.wait(lock);
            }

            if (m_state == quit) {
                break;
            }

            m_state = block;
        }

#if 0
		std::cout << "unblocking thread " << std::this_thread::get_id() << std::endl;
#endif

		// process until head == nullptr, i.e. each new add will
		// have to trigger a set_state(run) in Parallel::enqueue.

		while (true) {
			ParallelTask *head;

			{
				Spinlock spinlock(parallel);

				head = parallel->m_head;

				if (head == nullptr) {
					break;
				}

				head->busy += 1;
			}

			context.task = head;

			try {
				const size_t n = head->n;
				const ParallelTask::Lambda &lambda = head->lambda;

				while (true) {
					const size_t index = head->index.fetch_add(
						1, std::memory_order_relaxed);
					if (index >= n) {
						break;
					}
					lambda(index);
				}
			} catch(...) {
				std::cerr << "Parallel::Thread::work: uncaught error";
			}

			context.task = nullptr;

			parallel->release(head, false);
		}
	}
}

/*const SymbolState &symbol_state(const Symbol *symbol) {
	const Parallel::Context &c = Parallel::context();
	if (c.task) { // parallelize?
		return *c.task->symbol_state(c, symbol);
	} else {
		return symbol->m_user_state;
	}
}

SymbolState &mutable_symbol_state(const Symbol *symbol) {
	const Parallel::Context &c = Parallel::context();
	if (c.task) { // parallelize?
		return *c.task->mutable_symbol_state(c, symbol);
	} else {
		return symbol->m_user_state;
	}
}

void update_definitions_version(Definitions &definitions) {
	const Parallel::Context &context = Parallel::context();
	if (context.task) { // parallelize?
		return context.task->update_definitions_version(context);
	} else {
		return definitions.update_master_version();
	}
}

VersionRef definitions_version(const Definitions &definitions) {
	const Parallel::Context &context = Parallel::context();
	if (context.task) { // parallelize?
		return context.task->definitions_version(context);
	} else {
		return definitions.master_version();
	}
}*/