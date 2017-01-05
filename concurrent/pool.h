#include "combiner.h"

template<typename T, size_t PoolSize>
class MemoryPool { // not concurrent
private:
	using pool_size_t = uint16_t;

	static_assert(pool_size_t(PoolSize) == PoolSize, "PoolSize too large.");

	class MiniPool;

	struct Node {
		T instance;
		MiniPool *pool;
	};

	class MiniPool {
	public:
		MiniPool *prev;
		MiniPool *next;

		pool_size_t free[PoolSize];
		pool_size_t n;
		Node node[PoolSize];

		static MiniPool *create() {
			void * const block = ::operator new(sizeof(MiniPool));
			MiniPool * const pool = reinterpret_cast<MiniPool*>(block);
			pool->n = PoolSize;
			for (size_t i = 0; i < PoolSize; i++) {
				pool->free[i] = i;
				pool->node[i].pool = pool;
			}
			return pool;
		}

		static void destroy(MiniPool *pool) {
			::operator delete(pool);
		}
	};

	MiniPool *m_head;
	MiniPool *m_gc;

	inline void switch_pool(MiniPool *pool) {
		// dequeue current pool from m_head and install
		// the next or a new usable pool
		MiniPool * const next_pool = pool->next;

		if ((pool = next_pool) == nullptr) {
			pool = m_gc;
			if (pool) {
				m_gc = pool->next;
			} else {
				pool = MiniPool::create();
			}
			pool->next = nullptr;
			pool->prev = nullptr;
		} else {
			pool->prev = nullptr;
		}

		m_head = pool;
	}

public:
	MemoryPool() {
		m_head = MiniPool::create();
		m_head->next = nullptr;
		m_head->prev = nullptr;
		m_gc = nullptr;
	}

	inline T *allocate() {
		MiniPool * const pool = m_head;

		const size_t n = pool->n;
		if (n == 1) { // pool will be empty after taking 1?
			switch_pool(pool);
		}

		pool->n = n - 1;
		return &pool->node[pool->free[n - 1]].instance;
	}

	inline void free(T *instance) {
		Node *node = reinterpret_cast<Node*>(instance);
		MiniPool *pool = node->pool;

		const size_t n = pool->n;

		pool->free[n] = node - pool->node;
		pool->n = n + 1;

		if (n == 0) { // was full and removed from list?
			MiniPool *head = m_head;
			pool->prev = nullptr;
			pool->next = head;
			if (head) {
				head->prev = pool;
			}
			m_head = pool;
		} else if (n + 1 == PoolSize && (pool->prev || pool->next)) {
				// dequeue from m_head
				if (pool->prev) {
					pool->prev->next = pool->next;
				} else {
					m_head = pool->next;
				}
				if (pool->next) {
					pool->next->prev = pool->prev;
				}

				// enqueue into m_gc
				pool->next = m_gc;
				m_gc = pool;
			}
	}

	void gc() {

	}
};

template<typename T, uint16_t PoolSize = 1024>
class ObjectPool {
private:
	enum Command {
		Allocate,
		Free
	};

	class Pool {
	private:
		MemoryPool<T, PoolSize> m_pool;

	public:
		struct Argument {
			Command command;
			T *instance;
		};

		void operator()(Argument &r) {
			switch(r.command) {
				case Allocate:
					r.instance = m_pool.allocate();
					break;
				case Free:
					m_pool.free(r.instance);
					break;
			}
		}
	};

	Concurrent<Pool> m_pool;

public:
	template<typename... Args>
	T *allocate(Args... args) {
		typename Concurrent<Pool>::Argument argument;
		argument.command = Allocate;
		m_pool(argument);
		T * const instance = argument.instance;
		try {
			new(instance) T(args...);
		} catch(...) {
			argument.command = Free;
			m_pool(argument);
			throw;
		}
		return instance;
	}

	void free(T* instance) {
		try {
			instance->~T();
		} catch(...) {
			// should not get here.
		}

		m_pool.asynchronous([instance] (auto &argument) {
			argument.command = Free;
			argument.instance = instance;
		});
	}
};
