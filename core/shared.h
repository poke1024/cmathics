// @formatter:off

class SharedHeap {
public:
    template<typename T>
    inline static void release(const T *obj) {
        delete obj;
    }
};

class SharedPool {
public:
    template<typename T>
    inline static void release(const T *obj);
};

template<typename T, typename Owner>
class Shared;

template<typename T, typename Owner>
inline void intrusive_ptr_add_ref(const Shared<T, Owner> *obj) {
    ++obj->m_ref_count;
};

template<typename T, typename Owner>
inline void intrusive_ptr_release(const Shared<T, Owner> *obj) {
    if (--obj->m_ref_count == 0) {
        Owner::release(static_cast<const T*>(obj));
    }
};

template<typename T, typename Owner>
class Shared { // similar to boost::intrusive_ref_counter
protected:
    friend void ::intrusive_ptr_add_ref<T, Owner>(const Shared<T, Owner> *obj);
    friend void ::intrusive_ptr_release<T, Owner>(const Shared<T, Owner> *obj);

    mutable std::atomic<size_t> m_ref_count;

public:
    inline Shared() : m_ref_count(0) {
    }
};

// ConstSharedPtr is a SharedPtr that never changes what it's pointing to.

namespace thread_safe_intrusive_ptr {
	template<typename T, std::memory_order MemoryOrder>
	class intrusive_ptr;
};

template<typename T, std::memory_order MemoryOrder>
using SharedPtr = thread_safe_intrusive_ptr::intrusive_ptr<T, MemoryOrder>;

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

	template<std::memory_order MemoryOrder>
	ConstSharedPtr(const SharedPtr<T, MemoryOrder> &p);

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

	bool operator==(const ConstSharedPtr<T> &p) const {
		return get() == p.get();
	}

	bool operator!=(const ConstSharedPtr<T> &p) const {
		return get() != p.get();
	}
};

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
};

static_assert(sizeof(UnsafeSharedPtr<int>) == sizeof(ConstSharedPtr<int>),
	"illegal UnsafeSharedPtr structure size");

// SafePtr is a thread safe shared ptr in the vein of std::atomic_shared_ptr;
// the code is a copy of boost::intrusive_ptr with a few modifications.

namespace thread_safe_intrusive_ptr {

template<class T, std::memory_order MO> class intrusive_ptr
{
private:

    typedef intrusive_ptr this_type;

    template<bool add_ref=true>
    inline void set(T * const p) {
        if (add_ref && p) {
            intrusive_ptr_add_ref(p);
        }
        T * const q = px.exchange(p, MO);
        if (q) {
            intrusive_ptr_release(q);
        }
    }

public:

    typedef T element_type;

	intrusive_ptr(const ConstSharedPtr<T> &rhs) {
		T * const p = rhs.get();
		px.store(p, MO);
		if( p != 0 ) intrusive_ptr_add_ref( p );
	}

    intrusive_ptr() BOOST_NOEXCEPT : px( 0 )
    {
    }

    intrusive_ptr( T * p, bool add_ref = true ): px( p )
    {
        if( p != 0 && add_ref ) intrusive_ptr_add_ref( p );
    }

#if !defined(BOOST_NO_MEMBER_TEMPLATES) || defined(BOOST_MSVC6_MEMBER_TEMPLATES)

    template<class U>
#if !defined( BOOST_SP_NO_SP_CONVERTIBLE )

    intrusive_ptr( intrusive_ptr<U, MO> const & rhs, typename boost::detail::sp_enable_if_convertible<U,T>::type = boost::detail::sp_empty() )

#else

    intrusive_ptr( intrusive_ptr<U, MO> const & rhs )

#endif
    {
        T * const p = rhs.get();
        px.store(p, MO);
        if( p != 0 ) intrusive_ptr_add_ref( p );
    }

#endif

    intrusive_ptr(intrusive_ptr const & rhs)
    {
        T * const p = rhs.get();
        px.store(p, MO);
        if( p != 0 ) intrusive_ptr_add_ref( p );
    }

    ~intrusive_ptr()
    {
        T * const p = px.load(MO);
        if( p != 0 ) intrusive_ptr_release( p );
    }

#if !defined(BOOST_NO_MEMBER_TEMPLATES) || defined(BOOST_MSVC6_MEMBER_TEMPLATES)

    template<class U, std::memory_order UMO> intrusive_ptr & operator=(intrusive_ptr<U, UMO> const & rhs)
    {
        set(rhs.get());
        return *this;
    }

#endif

// Move support

#if !defined( BOOST_NO_CXX11_RVALUE_REFERENCES )

    intrusive_ptr(intrusive_ptr && rhs) BOOST_NOEXCEPT
    {
	    // move semantics should never happen across two threads.
        px.store(rhs.px.exchange(0, std::memory_order_relaxed));
    }

    intrusive_ptr & operator=(intrusive_ptr && rhs) BOOST_NOEXCEPT
    {
	    // move semantics should never happen across two threads.
        set<false>(static_cast<intrusive_ptr&&>(rhs).px.exchange(0, std::memory_order_relaxed));
        return *this;
    }

#endif

    intrusive_ptr & operator=(intrusive_ptr const & rhs)
    {
        set(rhs.px.load(MO));
        return *this;
    }

    intrusive_ptr & operator=(T * p)
    {
        set(p);
        return *this;
    }

    void reset() BOOST_NOEXCEPT
    {
        T * const q = px.exchange(0, MO);
        if (q) {
            intrusive_ptr_release(q);
        }
    }

    /*void reset( T * rhs )
    {
        this_type( rhs ).swap( *this );
    }

    void reset( T * rhs, bool add_ref )
    {
        this_type( rhs, add_ref ).swap( *this );
    }*/

    T * get() const BOOST_NOEXCEPT
    {
        return px.load(MO);
    }

    T * detach() BOOST_NOEXCEPT
    {
	    return px.exchange(0, MO);
    }

    T & operator*() const
    {
        BOOST_ASSERT( px != 0 );
        return *px.load(MO);
    }

    T * operator->() const
    {
        BOOST_ASSERT( px != 0 );
        return px.load(MO);
    }

	intrusive_ptr &operator=(const ConstSharedPtr<T> &v) {
		*this = v.get();
		return *this;
	}

// implicit conversion to "bool"
#include <boost/smart_ptr/detail/operator_bool.hpp>

private:

    std::atomic<T*> px;
};

template<class T, class U, std::memory_order MO> inline bool operator==(intrusive_ptr<T, MO> const & a, intrusive_ptr<U, MO> const & b)
{
    return a.get() == b.get();
}

template<class T, class U, std::memory_order MO> inline bool operator!=(intrusive_ptr<T, MO> const & a, intrusive_ptr<U, MO> const & b)
{
    return a.get() != b.get();
}

template<class T, class U, std::memory_order MO> inline bool operator==(intrusive_ptr<T, MO> const & a, U * b)
{
    return a.get() == b;
}

template<class T, class U, std::memory_order MO> inline bool operator!=(intrusive_ptr<T, MO> const & a, U * b)
{
    return a.get() != b;
}

template<class T, class U, std::memory_order MO> inline bool operator==(T * a, intrusive_ptr<U, MO> const & b)
{
    return a == b.get();
}

template<class T, class U, std::memory_order MO> inline bool operator!=(T * a, intrusive_ptr<U, MO> const & b)
{
    return a != b.get();
}

#if __GNUC__ == 2 && __GNUC_MINOR__ <= 96

// Resolve the ambiguity between our op!= and the one in rel_ops

template<class T> inline bool operator!=(intrusive_ptr<T> const & a, intrusive_ptr<T> const & b)
{
    return a.get() != b.get();
}

#endif

#if !defined( BOOST_NO_CXX11_NULLPTR )

template<class T, std::memory_order MO> inline bool operator==( intrusive_ptr<T, MO> const & p, boost::detail::sp_nullptr_t ) BOOST_NOEXCEPT
{
    return p.get() == 0;
}

template<class T, std::memory_order MO> inline bool operator==( boost::detail::sp_nullptr_t, intrusive_ptr<T, MO> const & p ) BOOST_NOEXCEPT
{
    return p.get() == 0;
}

template<class T, std::memory_order MO> inline bool operator!=( intrusive_ptr<T, MO> const & p, boost::detail::sp_nullptr_t ) BOOST_NOEXCEPT
{
    return p.get() != 0;
}

template<class T, std::memory_order MO> inline bool operator!=( boost::detail::sp_nullptr_t, intrusive_ptr<T, MO> const & p ) BOOST_NOEXCEPT
{
    return p.get() != 0;
}

#endif

template<class T, std::memory_order MO> inline bool operator<(intrusive_ptr<T, MO> const & a, intrusive_ptr<T, MO> const & b)
{
    return std::less<T *>()(a.get(), b.get());
}

/*template<class T> void swap(intrusive_ptr<T> & lhs, intrusive_ptr<T> & rhs)
{
    lhs.swap(rhs);
}*/

// mem_fn support

template<class T, std::memory_order MO> T * get_pointer(intrusive_ptr<T, MO> const & p)
{
    return p.get();
}

/*template<class T, class U> intrusive_ptr<T> static_pointer_cast(intrusive_ptr<U> const & p)
{
    return static_cast<T *>(p.get());
}

template<class T, class U> intrusive_ptr<T> const_pointer_cast(intrusive_ptr<U> const & p)
{
    return const_cast<T *>(p.get());
}

template<class T, class U> intrusive_ptr<T> dynamic_pointer_cast(intrusive_ptr<U> const & p)
{
    return dynamic_cast<T *>(p.get());
}*/

// operator<<

#if !defined(BOOST_NO_IOSTREAM)

#if defined(BOOST_NO_TEMPLATED_IOSTREAMS) || ( defined(__GNUC__) &&  (__GNUC__ < 3) )

template<class Y> std::ostream & operator<< (std::ostream & os, intrusive_ptr<Y> const & p)
{
    os << p.get();
    return os;
}

#else

// in STLport's no-iostreams mode no iostream symbols can be used
#ifndef _STLP_NO_IOSTREAMS

# if defined(BOOST_MSVC) && BOOST_WORKAROUND(BOOST_MSVC, < 1300 && __SGI_STL_PORT)
// MSVC6 has problems finding std::basic_ostream through the using declaration in namespace _STL
using std::basic_ostream;
template<class E, class T, class Y> basic_ostream<E, T> & operator<< (basic_ostream<E, T> & os, intrusive_ptr<Y> const & p)
# else
template<class E, class T, class Y, std::memory_order MO> std::basic_ostream<E, T> & operator<< (std::basic_ostream<E, T> & os, intrusive_ptr<Y, MO> const & p)
# endif
{
    os << p.get();
    return os;
}

#endif // _STLP_NO_IOSTREAMS

#endif // __GNUC__ < 3

#endif // !defined(BOOST_NO_IOSTREAM)

// hash_value

template<class T, std::memory_order MO> struct hash;

template<class T, std::memory_order MO> std::size_t hash_value( intrusive_ptr<T, MO> const & p )
{
    return boost::hash< T* >()( p.get() );
}

} // end of namespace thread_safe_intrusive_ptr

template<typename T, typename U, std::memory_order MemoryOrder>
inline SharedPtr<T, MemoryOrder> static_pointer_cast(const SharedPtr<U, MemoryOrder> &u) {
    return static_cast<T*>(u.get());
}

template<typename T, typename U, std::memory_order MemoryOrder>
inline SharedPtr<T, MemoryOrder> const_pointer_cast(const SharedPtr<U, MemoryOrder> &u) {
    return const_cast<T*>(u.get());
}

template<typename T>
template<std::memory_order MemoryOrder>
ConstSharedPtr<T>::ConstSharedPtr(const SharedPtr<T, MemoryOrder> &p) : m_ptr(p.get()) {
	if (m_ptr) {
		intrusive_ptr_add_ref(m_ptr);
	}
}

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

