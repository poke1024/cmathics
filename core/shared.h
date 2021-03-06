#pragma once
// @formatter:off

#include <thread>

template<typename T>
class ConstSharedPtr;

template<typename T>
class UnsafeSharedPtr;

template<typename T>
class SharedPtr {
protected:
    mutable std::atomic_flag m_lock = ATOMIC_FLAG_INIT;
	T *m_ptr;

public:
    SharedPtr() : m_ptr(nullptr) {
    }

    SharedPtr(T *ptr) : m_ptr(ptr) {
        if (ptr) {
            intrusive_ptr_add_ref(ptr);
        }
    }

    SharedPtr(const SharedPtr &p) {
        const ConstSharedPtr<T> const_p(p);
        m_ptr = const_p.get();
        if (m_ptr) {
            intrusive_ptr_add_ref(m_ptr);
        }
    }

	~SharedPtr() {
		if (m_ptr) {
			intrusive_ptr_release(m_ptr);
		}
	}

    operator ConstSharedPtr<T>() const {
        while (true) {
            if (m_lock.test_and_set(std::memory_order_acq_rel) == false) {
                ConstSharedPtr<T> p(m_ptr); // must not throw
                m_lock.clear(std::memory_order_release);
                return p;
            }
        }
    }

    SharedPtr &operator=(T *ptr) {
        if (ptr) {
            intrusive_ptr_add_ref(ptr);
        }
        while (true) {
            if (m_lock.test_and_set(std::memory_order_acq_rel) == false) {
                T *old = m_ptr;
                m_ptr = ptr;
                m_lock.clear(std::memory_order_release);
                if (old) {
                    intrusive_ptr_release(old);
                }
                break;
            }
        }
        return *this;
    }

    SharedPtr &operator=(const SharedPtr &p) {
        const ConstSharedPtr<T> const_p(p);
        *this = const_p.get();
        return *this;
    }

    template<typename U>
    SharedPtr &operator=(const ConstSharedPtr<U> &p) {
        *this = p.get();
        return *this;
    }
};

// a QuasiConstSharedPtr is a fully concurrent shared pointer, which only values the first
// non-nullptr assignment; all following assignments are ignored. why is this?

// imagine a fully concurrent SharedPtr instance p, and two threads accessing it:

// T1                               T2
// p = my_object;                   ... some work ...
// ...                              p = my_other_object;
// p->attribute = 1;

// obviously this would produce a data race; if "p = my_other_pointer" runs as shown
// above, the reference to my_pointer will get my_object, and, if it's the only ref,
// my_object will get freed. the following access to "p->attribute" would be invalid.
// in order to provide fully concurrent SharedPtrs, additional locks would be needed.

// a CachedPtr, once set, will always retain a value once set and never change it and
// never return to nullptr. that's why it's also safe to use which checks regarding its
// non-nullness.

// CachedPtr does not enforce seq cst ordering, but only acquire-release ordering. this
// means that, while it's ensured that once a thread sees a SharedPtr value, the data
// and object that is pointed to is fully available and consistent, the order in which
// the values of several CachedPtr are seen from different threads might be different.

// Having multiple CachedPtr instances set by one or several threads, a thread X might
// observe the new CachedPtr values in a different order than a thread Y; you cannot
// assume that several threads see the same global state of things with regard to
// multiple SafePtrs (for this, seq cst ordering is needed, which is slower).

// code using CachedPtr should thus not assume that another CachedPtr, that was set in
// the modifying code earlier originally, will be available in the expected state. code
// should instead only focus on single SharedPtr instances and rely on data structures
// like mutexes if a certain configuration of two or more CachedPtr is expected.

template<typename T>
class QuasiConstSharedPtr {
protected:
	std::atomic<T*> m_ptr;

public:
	QuasiConstSharedPtr() : m_ptr(nullptr) {
	}

	QuasiConstSharedPtr(T *ptr) : m_ptr(ptr) {
		if (ptr) {
			intrusive_ptr_add_ref(ptr);
		}
	}

   	QuasiConstSharedPtr(const ConstSharedPtr<T> &p) : m_ptr(p.get()) {
        T * const ptr = get();
		if (ptr) {
			intrusive_ptr_add_ref(ptr);
		}
    }

	QuasiConstSharedPtr(const QuasiConstSharedPtr<T> &p) : m_ptr(p.get()) {
        T * const ptr = get();
		if (ptr) {
			intrusive_ptr_add_ref(ptr);
		}
	}

	QuasiConstSharedPtr(QuasiConstSharedPtr<T> &&p) : m_ptr(p.get()) {
		// the caller guarantees that no other thread can access p.
		p.m_ptr = nullptr;
	}

	template<typename U>
	QuasiConstSharedPtr(const QuasiConstSharedPtr<U> &p) : m_ptr(p.get()) {
		if (m_ptr) {
			intrusive_ptr_add_ref(m_ptr);
		}
	}

	~QuasiConstSharedPtr() {
        T * const ptr = get();
		if (ptr) {
			intrusive_ptr_release(ptr);
		}
	}

    template<typename F, typename C>
    T *ensure(const F &f, const C &cond) {
        T *ptr = get();
        if (ptr && cond(ptr)) {
            return ptr;
        }

        T *actual = ptr;

        ConstSharedPtr<T> p = f();
        ptr = p.get();
        intrusive_ptr_add_ref(ptr);

        while (true) {
            if (m_ptr.compare_exchange_strong(actual, ptr, std::memory_order_acq_rel)) {
                if (actual) {
                    intrusive_ptr_release(actual);
                }
                return ptr;
            }
            if (actual && cond(actual)) {
                return actual;
            }
        }
    }

    template<typename F>
    T *ensure(const F &f) {
        return ensure(f, [] (T *ptr) {
            return true;
        });
    }

    QuasiConstSharedPtr &initialize(T *ptr) {
        T *expected = nullptr;
        if (m_ptr.compare_exchange_strong(expected, ptr, std::memory_order_acq_rel)) {
            if (ptr) {
                intrusive_ptr_add_ref(ptr);
            }
        } else {
            // initialize expects that value was not set before. there's something
            // wrong if we get here. maybe you want to use ensure() instead?
            assert(false);
        }
        return *this;
    }

    template<typename U>
    QuasiConstSharedPtr &initialize(const UnsafeSharedPtr<U> &p) {
        return initialize(p.get());
    }

    template<typename U>
    QuasiConstSharedPtr &initialize(const ConstSharedPtr<U> &p) {
        return initialize(p.get());
    }

    inline T *get() const {
		return m_ptr.load(std::memory_order_acquire);
	}

    T &operator*() const {
		return *m_ptr;
	}

	T *operator->() const {
		return m_ptr;
	}

	operator bool() const {
		return m_ptr != nullptr;
	}
};

template<typename T, typename U>
inline QuasiConstSharedPtr<T> static_pointer_cast(const QuasiConstSharedPtr<U> &u) {
	return static_cast<T*>(u.get());
}

template<typename T, typename U>
inline QuasiConstSharedPtr<T> const_pointer_cast(const QuasiConstSharedPtr<U> &u) {
	return const_cast<T*>(u.get());
}

// ConstSharedPtr is a shared ptr that never changes what it's pointing to.

template<typename T>
class ConstSharedPtr {
protected:
	T *m_ptr;

public:
	ConstSharedPtr() : m_ptr(nullptr) {
	}

	ConstSharedPtr(T *ptr) : m_ptr(ptr) {
		if (ptr) {
			intrusive_ptr_add_ref(ptr);
		}
	}

	ConstSharedPtr(const ConstSharedPtr<T> &p) : m_ptr(p.get()) {
		if (m_ptr) {
			intrusive_ptr_add_ref(m_ptr);
		}
	}

	ConstSharedPtr(ConstSharedPtr<T> &&p) : m_ptr(p.get()) {
		// the caller guarantees that no other thread can access p.
		p.m_ptr = nullptr;
	}

	template<typename U>
	ConstSharedPtr(const ConstSharedPtr<U> &p) : m_ptr(p.get()) {
		if (m_ptr) {
			intrusive_ptr_add_ref(m_ptr);
		}
	}

	ConstSharedPtr(const QuasiConstSharedPtr<T> &p);

	~ConstSharedPtr() {
		if (m_ptr) {
			intrusive_ptr_release(m_ptr);
		}
	}

	T &operator*() const {
		return *m_ptr;
	}

	T *operator->() const {
		return m_ptr;
	}

	T *get() const {
		return m_ptr;
	}

	operator bool() const {
		return m_ptr != nullptr;
	}

	bool operator==(const ConstSharedPtr &p) const {
		return get() == p.get();
	}

	bool operator!=(const ConstSharedPtr &p) const {
		return get() != p.get();
	}

	// only use the following unsafe_ functions if you know what you're doing!
	// this is only to be used in situations where you initialize an otherwise
	// uncontended data structure.

	void unsafe_mutate(ConstSharedPtr &&p) {
		m_ptr = p.m_ptr;
		p.m_ptr = nullptr;
	}

	void unsafe_swap(ConstSharedPtr &p) {
		std::swap(m_ptr, p.m_ptr);
	}
};

template<typename T>
ConstSharedPtr<T>::ConstSharedPtr(const QuasiConstSharedPtr<T> &p) : m_ptr(p.get()) {
	if (m_ptr) {
		intrusive_ptr_add_ref(m_ptr);
	}
}

template<typename T, typename U>
inline ConstSharedPtr<T> static_pointer_cast(const ConstSharedPtr<U> &u) {
	return static_cast<T*>(u.get());
}

template<typename T, typename U>
inline ConstSharedPtr<T> const_pointer_cast(const ConstSharedPtr<U> &u) {
	return const_cast<T*>(u.get());
}

template<typename T>
class UnsafeSharedPtr : public ConstSharedPtr<T> {
public:
	using ConstSharedPtr<T>::ConstSharedPtr;

	inline UnsafeSharedPtr &operator=(T *ptr) {
		T * const old_ptr = ConstSharedPtr<T>::m_ptr;
		if (ptr != old_ptr) {
			if (ptr) {
				intrusive_ptr_add_ref(ptr);
			}
			if (old_ptr) {
				intrusive_ptr_release(old_ptr);
			}
			ConstSharedPtr<T>::m_ptr = ptr;
		}
		return *this;
	}

	UnsafeSharedPtr &operator=(const ConstSharedPtr<T> &ptr) {
		*this = ptr.get();
		return *this;
	}

	UnsafeSharedPtr &operator=(const UnsafeSharedPtr<T> &ptr) {
		*this = ptr.get();
		return *this;
	}

	template<typename U>
	UnsafeSharedPtr &operator=(const ConstSharedPtr<U> &ptr) {
		*this = ptr.get();
		return *this;
	}

	void reset() {
		if (ConstSharedPtr<T>::m_ptr) {
			intrusive_ptr_release(ConstSharedPtr<T>::m_ptr);
		}
		ConstSharedPtr<T>::m_ptr = nullptr;
	}

	friend inline void swap(UnsafeSharedPtr& x, UnsafeSharedPtr& y) {
		std::swap(x.m_ptr, y.m_ptr);
	}
};

static_assert(sizeof(UnsafeSharedPtr<int>) == sizeof(ConstSharedPtr<int>),
	"illegal UnsafeSharedPtr structure size");

template<typename T>
class ThreadSharedPtr {
private:
	T *m_ptr[8];

	// FIXME: each thread gets its own pointers

public:
	void sync() {
		// call at start of a thread operation; syncs all n pointers to
		// the same global initial state.
	}
};

template<typename T>
class Spinlocked {
private:
	mutable std::atomic_flag m_lock = ATOMIC_FLAG_INIT;
	T m_data;

public:
	template<typename F>
	inline auto lock(const F &f)  {
		std::atomic_flag &lock = m_lock;
		while (lock.test_and_set(std::memory_order_acq_rel)) {
			std::this_thread::yield();
		}
		try {
			auto result = f(m_data);
			lock.clear(std::memory_order_release);
			return result;
		} catch(...) {
			lock.clear(std::memory_order_release);
			throw;
		}
	}

	template<typename F>
	inline auto lock_and_release_early(const F &f) const  {
		std::atomic_flag &lock = m_lock;
		while (true) {
			if (lock.test_and_set(std::memory_order_acq_rel) == false) {
				return f(m_data, [&lock] () {
					lock.clear(std::memory_order_release);
				});
			}
		}
	}
};

#include "concurrent/pool.h"

template<typename T>
inline void intrusive_ptr_add_ref(const T *obj);

template<typename T>
inline void intrusive_ptr_release(const T *obj);

class Shared { // similar to boost::intrusive_ref_counter
protected:
    template<typename T>
    friend void ::intrusive_ptr_add_ref(const T *obj);

    template<typename T>
    friend void ::intrusive_ptr_release(const T *obj);

    mutable std::atomic<size_t> m_ref_count;

public:
    inline Shared() : m_ref_count(0) {
    }

	virtual void destroy() = 0;
};

template<typename T>
class ExtendedHeapObject {
public:
	template<typename... Args>
	static ConstSharedPtr<T> construct(Args&&... args) {
		return new T(std::forward<Args>(args)...);
	}
};

template<typename T>
class AbstractHeapObject : public Shared {
public:
	virtual inline void destroy() {
		delete this;
	}
};

template<typename T>
class HeapObject :
    public AbstractHeapObject<T>,
    public ExtendedHeapObject<T> {
};

template<typename T>
class PoolObject : public virtual Shared {
private:
	static ObjectPool<T> s_pool;

public:
	virtual inline void destroy() final {
		s_pool.destroy(static_cast<T*>(this));
	}

	template<typename... Args>
	static ConstSharedPtr<T> construct(Args&&... args) {
		return s_pool.construct(std::forward<Args>(args)...);
	}
};

template<typename T>
ObjectPool<T> PoolObject<T>::s_pool;
