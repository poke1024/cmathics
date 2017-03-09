#ifndef CMATHICS_PARALLEL_H
#define CMATHICS_PARALLEL_H

#include <thread>
#include <list>
#include <mutex>

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

using ThreadNumber = uint16_t;

struct ParallelTask;

class Parallel {
public:
	enum {
		MaxParallelism = 8
	};

	using Lambda = std::function<void(size_t)>;

	class Barrier {
	private:
		static thread_local std::mutex t_mutex;
		static thread_local std::condition_variable t_condition;

		bool m_done;
		std::unique_lock<std::mutex> m_lock;

		std::mutex &m_mutex;
		std::condition_variable &m_condition;

	public:
		Barrier();

		void wait();

		void signal();
	};

	struct Context {
		// a number between 0 and n - 1 (n being the number of threads)
		// indicating the thread we're currently in. 0 is the main thread.
		// if no parallelize() is active, this will always be 0.
		ThreadNumber thread_number;

		// the task currently processed in the innermost parallelize().
		ParallelTask *task;

		// the parent context that this context's execution is embedded in.
		Context *parent;
	};

private:
	static Parallel *s_instance;

	static thread_local Context t_context;

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

	static inline const Context &context() {
		return t_context;
	}

	void parallelize(const Lambda &lambda, size_t n, const Evaluation &evaluation);
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
	        Parallel::Lambda lambda = [&f] (size_t i) {
		        f(i);
	        };
	        Parallel::instance()->parallelize(lambda, n, evaluation);
        }
    }
}

const SymbolState &symbol_state(const Symbol *symbol);

SymbolState &mutable_symbol_state(const Symbol *symbol);

void update_definitions_version(Definitions &definitions);

VersionRef definitions_version(const Definitions &definitions);

#endif // CMATHICS_PARALLEL_H
