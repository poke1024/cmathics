#include "combiner.h"
#include <iostream>
#include <unordered_map>

#define DEBUG_ALLOCATIONS 0

template<typename T, size_t PoolSize>
class MemoryPool { // not concurrent
private:
	using pool_size_t = uint16_t;

	static_assert(pool_size_t(PoolSize) == PoolSize, "PoolSize too large.");

	class MiniPool;

	struct Node {
		T instance;
		MiniPool *pool;
#if DEBUG_ALLOCATIONS
		uint64_t magic;
#endif
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
#if DEBUG_ALLOCATIONS
				pool->node[i].magic = 0xBADC0DED;
#endif
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
			if ((pool = m_gc) != nullptr) {
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

	struct Pile {
	protected:
		friend class MemoryPool;

		MiniPool *m_pool;
		pool_size_t m_free[PoolSize];
		int16_t m_index;

	public:
		template<typename Reallocate>
		inline T *allocate(const Reallocate &reallocate) {
			while (true) {
				const int16_t i = --m_index;

				if (i >= 0) {
					MiniPool * const pool = m_pool;
					Node * const node = &pool->node[m_free[i]];
#if DEBUG_ALLOCATIONS
					assert(node->magic == 0xBADC0DED);
					assert(node->pool == pool);
#endif
					return &node->instance;
				} else {
					reallocate();
				}
			}
		}

		template<typename Free>
		void clear(const Free &free) {
			MiniPool * const pool = m_pool;
			int16_t i = m_index;
			while (--i >= 0) {
				Node * const node = &pool->node[m_free[i]];
				free(&node->instance);
			}
			m_index = 0;
		}
	};

	inline void allocate(Pile * const pile) {
		MiniPool * const pool = m_head;

		const size_t n = pool->n;

		pile->m_pool = pool;
		std::memcpy(pile->m_free, pool->free, n * sizeof(pool_size_t));
		pile->m_index = n;

		pool->n = 0;

		switch_pool(pool);
	}

	inline void free(T *instance) {
		const Node * const node = reinterpret_cast<const Node*>(instance);
		MiniPool * const pool = node->pool;

#if DEBUG_ALLOCATIONS
		assert(node->magic == 0xBADC0DED);
#endif

		const size_t n = pool->n++;

		pool->free[n] = node - pool->node;

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

template<typename T, size_t PoolSize = 1024>
class ObjectPoolBase {
protected:
	using Pile = typename MemoryPool<T, PoolSize>::Pile;

	static thread_local Pile s_pile;

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
			switch (r.command) {
				case Allocate: {
					m_pool.allocate(&s_pile);
					break;
				}
				case Free:
					m_pool.free(r.instance);
					break;
			}
		}
	};

	Concurrent<Pool> m_pool;

public:
	inline T *allocate() {
		ObjectPoolBase * const self = this;
		return s_pile.allocate([self] () {
			typename Concurrent<Pool>::Argument argument;
			argument.command = Allocate;
			self->m_pool(argument);
		});
	}

	inline void free(T *instance) {
		m_pool.asynchronous([instance] (auto &argument) {
			argument.command = Free;
			argument.instance = instance;
		});
	}

	~ObjectPoolBase() {
		ObjectPoolBase * const self = this;
		s_pile.clear([self] (T *instance) {
			self->free(instance);
		});
	}
};

template<typename T, size_t PoolSize>
thread_local typename ObjectPoolBase<T, PoolSize>::Pile ObjectPoolBase<T, PoolSize>::s_pile;

template<typename T, size_t PoolSize = 1024>
class ObjectPool : protected ObjectPoolBase<T, PoolSize> {
private:
	using Base = ObjectPoolBase<T, PoolSize>;
	using Pool = typename Base::Pool;

public:
	template<typename... Args>
	T *construct(const Args&... args) {
		T * const instance = Base::allocate();
		try {
			new(instance) T(args...);
		} catch(...) {
			Base::free(instance);
			throw;
		}
		return instance;
	}

	void destroy(T* instance) {
		try {
			instance->~T();
		} catch(...) {
			std::cerr << "destructor threw an exception.";
		}

		Base::free(instance);
	}
};

template<typename T, size_t PoolSize = 1024>
class ObjectAllocator : public std::allocator<T> {
private:
	using Base = ObjectPoolBase<T, PoolSize>;

	const std::shared_ptr<Base> m_base;

public:
	using value_type = T;

	ObjectAllocator() : m_base(std::make_shared<Base>()) {
	}

	ObjectAllocator(const ObjectAllocator &allocator) : m_base(allocator.m_base) {
	}

	T *allocate(size_t n, const void *hint = 0) {
		assert(n == 1);
		return m_base->allocate();
	}

	void deallocate(T *p, size_t n) {
		assert(n == 1);
		m_base->free(p);
	}
};

template<typename T, size_t PoolSize = 8>
class VectorAllocator : public std::allocator<T> {
private:
	static constexpr int nbits = 8 * sizeof(unsigned int);

	struct Node {
		Node *next;
		T block[0];
	};

	struct Pools {
		std::atomic<Node*> heads[nbits];
	};

	const std::shared_ptr<Pools> m_pools;

	inline static int bits(size_t n) {
		const int k = (nbits - __builtin_clz(n)) - 1;
#if DEBUG_ALLOCATIONS
		assert(k >= 0 && k < nbits);
		assert(n <= (1LL << k));
#endif
		return k;
	}

public:
	using value_type = T;

	VectorAllocator() : m_pools(std::make_shared<Pools>()) {
		for (int i = 0; i < nbits; i++) {
			m_pools->heads[i].store(nullptr, std::memory_order_release);
		}
	}

	VectorAllocator(const VectorAllocator &allocator) : m_pools(allocator.m_pools) {
	}

	T *allocate(size_t n, const void *hint = 0) {
		if (n == 0) {
			return nullptr;
		}

		const int k = bits(n);

		std::atomic<Node*> &head = m_pools->heads[k];

		Node *node = head.load(std::memory_order_acquire);
		while (node) {
			if (head.compare_exchange_strong(node, node->next, std::memory_order_acq_rel)) {
				return node->block;
			}
		}

		node = reinterpret_cast<Node*>(
			::operator new(sizeof(Node) + sizeof(T) * (1LL << k)));

		return node->block;
	}

	void deallocate(T *p, size_t n) {
		const int k = bits(n);

		Node * const node = reinterpret_cast<Node*>(
			reinterpret_cast<uint8_t*>(p) - offsetof(Node, block));

		std::atomic<Node*> &head = m_pools->heads[k];

		Node *next = head.load(std::memory_order_acquire);
		while (true) {
			node->next = next;
			if (head.compare_exchange_strong(next, node, std::memory_order_acq_rel)) {
				break;
			}
		}
	}
};
