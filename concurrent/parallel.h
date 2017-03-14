#ifndef CMATHICS_PARALLEL_H
#define CMATHICS_PARALLEL_H

#include <thread>
#include <list>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

class Definitions;
class Evaluation;

class Symbol;
class SymbolState;

class Version : public PoolObject<Version> {
private:
	const ConstSharedPtr<Version> m_master;

public:
	inline Version() {
	}

	inline Version(const ConstSharedPtr<Version> &master) : m_master(master) {
	}

	inline const auto &master() const {
		return m_master;
	}

	bool equivalent_to(const Version *version) const;
};

using VersionRef = ConstSharedPtr<Version>;
using UnsafeVersionRef = UnsafeSharedPtr<Version>;

using ThreadNumber = uint16_t;

class ParallelTask;

struct ParallelContext {
	// a number between 0 and n - 1 (n being the number of threads)
	// indicating the thread we're currently in. 0 is the main thread.
	// if no parallelize() is active, this will always be 0.
	ThreadNumber thread_number;

	// the task currently processed in the innermost parallelize().
	ParallelTask *task;

	// the parent context that this context's execution is embedded in.
	ParallelContext *parent;
};

class TaskLocalStorageBase {
protected:
	friend class ParallelTask;

	virtual void remove_task(ParallelTask *task) = 0;
};

class Spinlock {
private:
	std::atomic_flag &m_mutex;

public:
	inline Spinlock(std::atomic_flag &mutex) : m_mutex(mutex) {
		while (m_mutex.test_and_set(std::memory_order_acq_rel)) {
		}
	}

	inline ~Spinlock() {
		m_mutex.clear(std::memory_order_release);
	}
};

template<typename T>
class TaskLocalStorage : public TaskLocalStorageBase {
private:
	mutable std::atomic_flag m_mutex = ATOMIC_FLAG_INIT;
	std::unordered_map<ParallelTask*, T> m_states;
	T m_master_state;

	inline const T &get(const ParallelContext *context) const;

protected:
	virtual void remove_task(ParallelTask *task) {
		Spinlock lock(m_mutex);
		m_states.erase(task);
	}

public:
	template<typename... Args>
	inline TaskLocalStorage(Args&&... args) : m_master_state(std::forward<Args>(args)...) {
	}

	inline ~TaskLocalStorage();

	inline const T &get() const;

	inline T &modify();

	inline T &set(const T &element);

	inline const T &get_master() const {
		return m_master_state;
	}

	inline void set_master(const T &state) {
		m_master_state = state;
	}
};

class ParallelBarrier {
private:
	static thread_local std::mutex t_mutex;
	static thread_local std::condition_variable t_condition;

	bool m_done;
	std::unique_lock<std::mutex> m_lock;

	std::mutex &m_mutex;
	std::condition_variable &m_condition;

public:
	ParallelBarrier();

	void wait();

	void signal();
};

class ParallelTask {
private:
	mutable std::atomic_flag m_mutex = ATOMIC_FLAG_INIT;
	std::unordered_set<TaskLocalStorageBase*> m_storages;

protected:
	friend class TaskLocalStorageBase;

	template<typename T>
	friend class TaskLocalStorage;

	void register_storage(TaskLocalStorageBase *storage) {
		Spinlock lock(m_mutex);
		m_storages.insert(storage);
	}

	void unregister_storage(TaskLocalStorageBase *storage) {
		Spinlock lock(m_mutex);
		m_storages.erase(storage);
	}

public:
	using Lambda = std::function<void(size_t)>;

	ParallelTask *prev;
	ParallelTask *next;
	bool enqueued;

	volatile int16_t busy;
	std::atomic<size_t> index;

	const Lambda &lambda;
	const size_t n;

	const Evaluation &evaluation;
	const VersionRef base_version;

	ParallelBarrier *barrier;

	/*inline const SymbolState *symbol_state(const ParallelContext &context, const Symbol *symbol) const;

	inline SymbolState *mutable_symbol_state(const ParallelContext &context, const Symbol *symbol);

	inline VersionRef definitions_version(const Parallel::Context &context) const;

	inline void update_definitions_version(const Parallel::Context &context);*/

	inline ParallelTask(const Lambda &lambda_, size_t n_, const VersionRef &version_, const Evaluation &evaluation_) :
		lambda(lambda_), n(n_), base_version(version_), evaluation(evaluation_), busy(1), enqueued(false) {

		index.store(0, std::memory_order_relaxed);
	}

	inline ~ParallelTask() {
		Spinlock lock(m_mutex);
		for (TaskLocalStorageBase *storage : m_storages) {
			storage->remove_task(this);
		}
	}
};

class Parallel {
public:
	enum {
		MaxParallelism = 8
	};

private:
	static Parallel *s_instance;

	static thread_local ParallelContext t_context;

	enum ThreadState {
		run,
		block,
		quit
	};

	class Thread {
	private:
		Parallel * const m_parallel;
		std::mutex m_mutex;
		std::condition_variable m_event;
		ThreadState m_state;
		std::thread m_thread;
		const ThreadNumber m_thread_number;

	public:
		Thread(Parallel *parallel, ThreadNumber thread_number);

		~Thread();

		void set_state(ThreadState state);

		void work();

        inline std::thread::id get_id() const {
            return m_thread.get_id();
        }
	};

	class Spinlock {
	private:
		Parallel * const m_parallel;
		bool m_acquired;

	public:
		Spinlock(Parallel *parallel);
		~Spinlock();

        void acquire();

		void release();
	};

	std::list<std::unique_ptr<Thread>> m_threads;

	std::atomic_flag m_lock = ATOMIC_FLAG_INIT;

	ParallelTask *m_head;
	std::atomic<int16_t> m_size;

private:
	Parallel();

	~Parallel();

	bool enqueue(ParallelTask *task);

	void release(ParallelTask *task, bool owner);

public:
	static void init();

	static void shutdown();

	static inline Parallel *instance() {
		return s_instance;
	}

	static inline const ParallelContext &context() {
		return t_context;
	}

	void parallelize(const ParallelTask::Lambda &lambda, size_t n, const Evaluation &evaluation);
};

template<typename F>
inline void parallelize(const F &f, size_t n, const Evaluation &evaluation) {
    if (n > 0) {
        if (n == 1) {
            f(0);
        } else {
	        // the following assignment assures, that the
	        // Parallel::Lambda (i.e. std::function) we
	        // generate here only has one capture and thus
	        // does not need dynamic allocation.
	        ParallelTask::Lambda lambda = [&f] (size_t i) {
		        f(i);
	        };
	        Parallel::instance()->parallelize(lambda, n, evaluation);
        }
    }
}

/*const SymbolState &symbol_state(const Symbol *symbol);

SymbolState &mutable_symbol_state(const Symbol *symbol);

void update_definitions_version(Definitions &definitions);

VersionRef definitions_version(const Definitions &definitions);*/

template<typename T>
inline TaskLocalStorage<T>::~TaskLocalStorage() {
	Spinlock lock(m_mutex);
	for (auto i = m_states.begin(); i != m_states.end(); i++) {
		i->first->unregister_storage(this);
	}
}

template<typename T>
const T &TaskLocalStorage<T>::get(const ParallelContext *context) const {
	Spinlock lock(m_mutex);
	while (true) {
		ParallelTask *task = context->task;
		if (task) {
			if (m_states.find(task) != m_states.end()) {
				return m_states.at(task);
			} else {
				if (context->parent) {
					context = context->parent;
				} else {
					return m_master_state;
				}
			}
		} else {
			return m_master_state;
		}
	}
}

template<typename T>
const T &TaskLocalStorage<T>::get() const {
	return get(&Parallel::context());
}

template<typename T>
T &TaskLocalStorage<T>::modify() {
	const ParallelContext &context = Parallel::context();
	ParallelTask *task = context.task;
	if (task) {
		{
			Spinlock lock(m_mutex);
			if (m_states.find(task) != m_states.end()) {
				return m_states.at(task);
			}
		}
		if (context.parent) {
			return set(get(context.parent));
		} else {
			return m_master_state;
		}
	} else {
		return m_master_state;
	}
}

template<typename T>
inline T &TaskLocalStorage<T>::set(const T &element) {
	ParallelTask *task = Parallel::context().task;
	if (task) {
		Spinlock lock(m_mutex);
		 m_states.emplace(task, element);
		task->register_storage(this);
		return m_states.at(task);
	} else {
		m_master_state = element;
		return m_master_state;
	}
}

#endif // CMATHICS_PARALLEL_H
