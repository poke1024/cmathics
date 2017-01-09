#include <thread>
#include <list>

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

		int16_t busy;
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

	public:
		Thread(Parallel *parallel);

		~Thread();

		void set_state(ThreadState state);

		void work();
	};

	class Spinlock {
	private:
		Parallel * const m_parallel;
		bool m_acquired;

	public:
		Spinlock(Parallel *parallel);
		~Spinlock();

		void clear();
	};

	std::list<std::unique_ptr<Thread>> m_threads;

	std::atomic_flag m_lock = ATOMIC_FLAG_INIT;

	Task *m_head;
	int16_t m_size;

private:
	Parallel();

	~Parallel();

	bool enqueue(Task *task);

	void release(Task *task, bool owner);

public:
	static void init();

	static void shutdown();

	static inline Parallel *instance() {
		return s_instance;
	}

	void parallelize(const Lambda &lambda, size_t n);
};

inline void parallelize(const Parallel::Lambda &lambda, size_t n) {
	Parallel::instance()->parallelize(lambda, n);
}
