#include "combiner.h"
#include <iostream>
#include <unordered_map>

#define DEBUG_ALLOCATIONS 0

template<typename T>
class Queue {
private:
    std::atomic<T*> m_head;

public:
    Queue() : m_head(nullptr) {
    }

    inline void enqueue(T *item) {
        T *link = m_head.load(std::memory_order_acquire);
        while (true) {
	        assert(item != link);
	        item->next = link;
            if (m_head.compare_exchange_weak(link, item, std::memory_order_acq_rel)) {
                break;
            }
        }
    }

    inline T *dequeue() {
	    T *item;
	    if ((item = m_head.load(std::memory_order_acquire)) == nullptr) {
		    return nullptr;
	    }

	    item->lock();
        while (true) {
	        T *stale = item;

            if (m_head.compare_exchange_weak(item, item->next, std::memory_order_acq_rel)) {
	            item->unlock();
                return item;
            }

	        if (item != stale) {
		        stale->unlock();
		        if (item == nullptr) {
			        break;
		        }
		        item->lock();
	        }
        }

	    return nullptr;
    }

#if DEBUG_ALLOCATIONS
    inline T* front() {
        return m_head.load(std::memory_order_acquire);
    }
#endif
};

template<typename T, size_t PoolSize>
class MemoryPool { // not concurrent
public:
    class MiniPool;

	using pool_size_t = uint16_t;

    struct Node {
        uint8_t instance[sizeof(T)];
        MiniPool *pool;
        Node *next;
#if DEBUG_ALLOCATIONS
        uint64_t magic;
#endif
    };

	static_assert(pool_size_t(PoolSize) == PoolSize, "PoolSize too large.");

    class Pile;

	enum MiniPoolState {
		REACTIVATE,
		EXHAUSTED,
		FREE,
		GC
	};

	// NOTE: a wrong alignment of MiniPool::data quadruples the runtime of
	// the whole system!

	class MiniPool {
	public:
		Node data[PoolSize];

        MiniPool *next;

		MiniPoolState state;
        Node *free;
		pool_size_t n;
		std::atomic_flag spinlock = ATOMIC_FLAG_INIT;

        inline MiniPool() : state(REACTIVATE), n(PoolSize) {
            Node *head = nullptr;

			for (size_t i = 0; i < PoolSize; i++) {
                Node * const node = &data[i];

                node->next = head;
                head = node;

				node->pool = this;

#if DEBUG_ALLOCATIONS
				node->magic = 0xBADC0DED;
#endif
			}

	        free = head;

#if DEBUG_ALLOCATIONS
            Node *node = head;
            while (node) {
                assert(node->magic == 0xBADC0DED);
                node = node->next;
            }
#endif
		}

		inline Node *grab() noexcept {
#if DEBUG_ALLOCATIONS
			assert(free->magic == 0xBADC0DED);
#endif
			assert(state == REACTIVATE);
			Node * const node = free;
			free = nullptr;
			n = 0;
			state = EXHAUSTED;
			return node;
		}

		inline void lock() noexcept {
			while (spinlock.test_and_set(std::memory_order_acq_rel)) {
				// wait
			}
		}

		inline void unlock() noexcept {
			spinlock.clear(std::memory_order_release);
		}
    };

private:
    Queue<MiniPool> m_pools;
    Queue<MiniPool> m_gc;

public:
	struct Pile {
	protected:
		friend class MemoryPool;

		Node *m_free;

	public:
        inline void initialize(Node *head) {
#if DEBUG_ALLOCATIONS
			assert(head);

            Node *node = head;
            while (node) {
                assert(node->magic == 0xBADC0DED);
                node = node->next;
            }
#endif

            m_free = head;
        }

		template<typename Reallocate>
		inline T *allocate(const Reallocate &reallocate) {
			while (true) {
				Node *node;

				if ((node = m_free) != nullptr) {
                    m_free = node->next;
#if DEBUG_ALLOCATIONS
					assert(node->magic == 0xBADC0DED);
					//assert(node->pool == pool);
#endif
					return reinterpret_cast<T*>(node->instance);
				} else {
					reallocate();
				}
			}
		}

		template<typename Free>
		void clear(const Free &free) {
			while (m_free) {
				Node * const next = m_free->next;
				free(reinterpret_cast<T*>(m_free->instance));
                m_free = next;
			}
		}
	};

	inline bool allocate(Pile * const pile) {
        while (true) {
            MiniPool *pool = m_pools.dequeue();

            if (pool) {
	            pool->lock();

	            if (pool->state == FREE) {
		            pool->state = GC;
		            m_gc.enqueue(pool);
		            pool->unlock();
		            continue;
	            }

	            assert(pool->state == REACTIVATE);

				Node * const node = pool->grab();
				pool->unlock();

				pile->initialize(node);
				return true;
            } else {
                if ((pool = m_gc.dequeue()) != nullptr) {
	                Node *node = nullptr;

	                pool->lock();
	                assert(pool->state == GC);

	                pool->state = REACTIVATE;
	                node = pool->grab();

	                pool->unlock();

					if (node) {
						pile->initialize(node);
						return true;
					}
                } else {
	                return false;
                }
            }
        }
	}

	inline void free(Node *head, Node *tail, size_t k) {
        MiniPool * const pool = head->pool;

#if DEBUG_ALLOCATIONS
        size_t count = 0;
        Node *node = head;

        while (node) {
            assert(node->magic == 0xBADC0DED);
            assert(node->pool = pool);

            count += 1;
            node = node->next;
        }

        assert(count == k);
#endif

		pool->lock();

		tail->next = pool->free;
		pool->free = head;
		const size_t n = pool->n;
		pool->n = n + k;

		if (n + k == PoolSize) {
			assert(pool->state == EXHAUSTED || pool->state == REACTIVATE);
			pool->state = FREE;
			pool->unlock();
		} else if (n == 0) {
			assert(pool->state == EXHAUSTED);
			pool->state = REACTIVATE;
			m_pools.enqueue(pool);
			pool->unlock();
		} else {
			assert(pool->state == REACTIVATE);
			pool->unlock();
		}
	}

	void gc() {

	}
};

template<typename T, size_t PoolSize>
class ObjectPoolBase {
public:
    enum Command {
        AllocateCommand,
        FreeCommand
    };

    using Pool = MemoryPool<T, PoolSize>;

    using Node = typename Pool::Node;

    using MiniPool = typename Pool::MiniPool;

    using pool_size_t = typename Pool::pool_size_t;

    class Free {
    private:
        Node *m_head;
        Node *m_tail;
        pool_size_t m_size;

    public:
        Free() : m_head(nullptr), m_size(0) {
        }

        inline void operator()(T *instance, Pool &pool) {
            Node * const node = reinterpret_cast<Node*>(instance);

            Node * head = m_head;

            if (head) {
                if (node->pool != head->pool) {
                    flush(pool);
                    head = nullptr;
                    m_tail = node;
                }
            } else {
                m_tail = node;
            }

            node->next = head;
            m_head = node;

            if (++m_size >= PoolSize) {
                flush(pool);
            }
        }

        inline void flush(Pool &pool) {
	        if (m_head) {
		        pool.free(m_head, m_tail, m_size);

		        m_head = nullptr;
		        m_size = 0;
	        }
        }
    };

protected:
	using Pile = typename Pool::Pile;

	static thread_local Pile s_pile;
    static thread_local Free s_free;

    Pool m_pool;

public:
	inline T *allocate() {
		ObjectPoolBase * const self = this;
        Pile *pile = &s_pile;

		return pile->allocate([self, pile] () {
            if (!self->m_pool.allocate(pile)) {
                MiniPool * const pool = new MiniPool;
                pile->initialize(pool->grab());
            }
		});
	}

	inline void free(T *instance) {
        s_free(instance, m_pool);
	}

	~ObjectPoolBase() {
		ObjectPoolBase * const self = this;
		s_pile.clear([self] (T *instance) {
			self->free(instance);
		});
        s_free.flush(m_pool);
	}
};

template<typename T, size_t PoolSize>
thread_local typename ObjectPoolBase<T, PoolSize>::Pile ObjectPoolBase<T, PoolSize>::s_pile;

template<typename T, size_t PoolSize>
thread_local typename ObjectPoolBase<T, PoolSize>::Free ObjectPoolBase<T, PoolSize>::s_free;

template<typename T, size_t PoolSize = 1024>
class ObjectPool : protected ObjectPoolBase<T, PoolSize> {
private:
	using Base = ObjectPoolBase<T, PoolSize>;
	using Pool = typename Base::Pool;

public:
	template<typename... Args>
	inline T *construct(const Args&... args) {
		T * const instance = Base::allocate();
		try {
			new(instance) T(args...);
		} catch(...) {
			Base::free(instance);
			throw;
		}
		return instance;
	}

	template<typename X>
	inline T *construct(X&& x) {
		T * const instance = Base::allocate();
		try {
			new(instance) T(std::move(x));
		} catch(...) {
			Base::free(instance);
			throw;
		}
		return instance;
	}

	inline void destroy(T* instance) {
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
