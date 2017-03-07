#ifndef CMATHICS_PARALLEL_H
#define CMATHICS_PARALLEL_H

#include <thread>
#include <list>
#include <mutex>

class ParallelExecution : public PoolObject<ParallelExecution> {
};

using ParallelExecutionId = ConstSharedPtr<ParallelExecution>;

class Parallel {
public:
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

	struct Task {
		Task *prev;
		Task *next;
		bool enqueued;

		volatile int16_t busy;
		std::atomic<size_t> index;

		const Lambda &lambda;
		const size_t n;

		Barrier *barrier;

		inline Task(const Lambda &lambda_, size_t n_) : lambda(lambda_), n(n_), busy(1), enqueued(false) {
			index.store(0, std::memory_order_relaxed);
		}
	};

private:
	static Parallel *s_instance;

	using ThreadNumber = int16_t;

	static thread_local ThreadNumber t_thread_number;
	static UnsafeSharedPtr<ParallelExecution> s_execution_id;

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

	Task *m_head;
	std::atomic<int16_t> m_size;

private:
	Parallel();

	~Parallel();

	bool enqueue(Task *task);

	void release(Task *task, bool owner);

	void execute(const Lambda &lambda, size_t n);

public:
	static void init();

	static void shutdown();

	static inline Parallel *instance() {
		return s_instance;
	}

	void parallelize(const Lambda &lambda, size_t n);

	static inline ThreadNumber thread_number() {
		// returns a number between 0 and n - 1 (n being the number of
		// threads) indicating the thread we're currently in. 0 is the
		// main thread. if no parallelize() is active, this will always
		// be 0.

		return t_thread_number;
	}

	static inline ParallelExecutionId execution_id() {
		// returns a unique identifier for the current parallel execution
		// started by the outermost parallelize().
		// note that s_execution_id is safe to access here as it's only
		// written at the start of parallelize() when no other threads
		// than the main threads are running.

		return s_execution_id;
	}
};

template<typename F>
inline void parallelize(const F &f, size_t n) {
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
	        Parallel::instance()->parallelize(lambda, n);
        }
    }
}

#endif // CMATHICS_PARALLEL_H
