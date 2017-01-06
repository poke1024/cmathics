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

	inline T *allocate() {
		MiniPool * const pool = m_head;

		const size_t r = --pool->n;
		if (r == 0) { // pool will be empty after taking 1?
			switch_pool(pool);
		}

#if DEBUG_ALLOCATIONS
		assert(r >= 0 && r < PoolSize);
		assert(pool->node[pool->free[r]].pool == pool);
#endif

		return &pool->node[pool->free[r]].instance;
	}

	inline void free(T *instance) {
		const Node * const node = reinterpret_cast<const Node*>(instance);
		MiniPool * const pool = node->pool;

#if DEBUG_ALLOCATIONS
		assert(node->magic == 0xBADC0DED);
#endif

		const size_t n = ++pool->n;

		const size_t k = n - 1;
		pool->free[k] = node - pool->node;

		if (k == 0) { // was full and removed from list?
			MiniPool *head = m_head;
			pool->prev = nullptr;
			pool->next = head;
			if (head) {
				head->prev = pool;
			}
			m_head = pool;
		} else if (n == PoolSize && (pool->prev || pool->next)) {
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
public:
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
};

template<typename T, size_t PoolSize = 1024>
class ObjectPool : protected ObjectPoolBase<T, PoolSize> {
private:
	using Base = ObjectPoolBase<T, PoolSize>;
	using Pool = typename Base::Pool;

public:
	template<typename... Args>
	T *construct(const Args&... args) {
		typename Concurrent<Pool>::Argument argument;
		argument.command = Base::Allocate;
		Base::m_pool(argument);
		T * const instance = argument.instance;
		try {
			new(instance) T(args...);
		} catch(...) {
			argument.command = Base::Free;
			Base::m_pool(argument);
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

		Base::m_pool.asynchronous([instance] (auto &argument) {
			argument.command = Base::Free;
			argument.instance = instance;
		});
	}
};

template<typename T, size_t PoolSize = 1024>
class ObjectAllocator : public std::allocator<T> {
private:
	using Base = ObjectPoolBase<T, PoolSize>;
	using Pool = typename Base::Pool;

	const std::shared_ptr<Base> m_base;

public:
	using value_type = T;

	ObjectAllocator() : m_base(std::make_shared<Base>()) {
	}

	ObjectAllocator(const ObjectAllocator &allocator) : m_base(allocator.m_base) {
	}

	T *allocate(size_t n, const void *hint = 0) {
		assert(n == 1);
		typename Concurrent<Pool>::Argument argument;
		argument.command = Base::Allocate;
		m_base->m_pool(argument);
		return argument.instance;
	}

	void deallocate(T *p, size_t n) {
		assert(n == 1);
		m_base->m_pool.asynchronous([p] (auto &argument) {
			argument.command = Base::Free;
			argument.instance = p;
		});
	}
};

template<typename T, size_t PoolSize = 8>
class VectorAllocator : public std::allocator<T> {
private:
	using Base = ObjectPoolBase<T, PoolSize>;
	using Pool = typename Base::Pool;

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
		assert(k >= 0 && k < nbits);
		assert(n <= (1LL << k));
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
