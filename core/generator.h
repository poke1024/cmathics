#ifndef CMATHICS_GENERATOR_H
#define CMATHICS_GENERATOR_H

class SwappableBaseExpressionRef {
private:
	BaseExpressionRef *m_ref;

public:
	inline SwappableBaseExpressionRef() : m_ref(nullptr) {
	}

	inline SwappableBaseExpressionRef(BaseExpressionRef &ref) : m_ref(&ref) {
	}

	inline SwappableBaseExpressionRef &operator=(const SwappableBaseExpressionRef &ref) {
		m_ref = ref.m_ref;
		return *this;
	}

	inline BaseExpressionPtr operator->() const {
		return m_ref->get();
	}

	friend void swap(SwappableBaseExpressionRef a, SwappableBaseExpressionRef b) {
		a.m_ref->unsafe_swap(*b.m_ref);
	}
};

class MutableIterator {
private:
	std::vector<BaseExpressionRef>::iterator m_iterator;

public:
	using iterator_type = std::vector<BaseExpressionRef>::iterator;

	inline MutableIterator() {
	}

	inline MutableIterator(const std::vector<BaseExpressionRef>::iterator &iterator) :
		m_iterator(iterator) {
	}

	inline MutableIterator &operator=(const MutableIterator &i) {
		m_iterator = i.m_iterator;
		return *this;
	}

	inline bool operator==(const MutableIterator &i) const {
		return m_iterator == i.m_iterator;
	}

	inline bool operator>=(const MutableIterator &i) const {
		return m_iterator >= i.m_iterator;
	}

	inline bool operator>(const MutableIterator &i) const {
		return m_iterator > i.m_iterator;
	}

	inline bool operator<=(const MutableIterator &i) const {
		return m_iterator <= i.m_iterator;
	}

	inline bool operator<(const MutableIterator &i) const {
		return m_iterator < i.m_iterator;
	}

	inline bool operator!=(const MutableIterator &i) const {
		return m_iterator != i.m_iterator;
	}

	inline SwappableBaseExpressionRef operator[](size_t i) const {
		return SwappableBaseExpressionRef(m_iterator[i]);
	}

	inline SwappableBaseExpressionRef operator*() const {
		return SwappableBaseExpressionRef(*m_iterator);
	}

	inline auto operator-(const MutableIterator &i) const {
		return m_iterator - i.m_iterator;
	}

	inline MutableIterator operator--(int) {
		MutableIterator previous(*this);
		--m_iterator;
		return previous;
	}

	inline MutableIterator &operator--() {
		--m_iterator;
		return *this;
	}

	inline MutableIterator operator++(int) {
		MutableIterator previous(*this);
		++m_iterator;
		return previous;
	}

	inline MutableIterator &operator++() {
		++m_iterator;
		return *this;
	}

	inline MutableIterator operator+(size_t i) const {
		return MutableIterator(m_iterator + i);
	}

	inline MutableIterator &operator+=(size_t i) {
		m_iterator += i;
		return *this;
	}

	inline MutableIterator operator-(size_t i) const {
		return MutableIterator(m_iterator - i);
	}

	inline MutableIterator &operator-=(size_t i) {
		m_iterator -= i;
		return *this;
	}
};

namespace std {
	template<>
	struct iterator_traits<MutableIterator> {
		using difference_type = MutableIterator::iterator_type::difference_type;
		using value_type = SwappableBaseExpressionRef;
	};
}

template<typename Allocator>
class LeafVectorBase {
private:
	std::vector<BaseExpressionRef, Allocator> m_leaves;
	TypeMask m_mask;

public:
	inline LeafVectorBase() : m_mask(0) {
	}

	inline LeafVectorBase(std::vector<BaseExpressionRef, Allocator> &&leaves, TypeMask mask) :
		m_leaves(leaves), m_mask(mask) {
	}

	inline LeafVectorBase(std::vector<BaseExpressionRef, Allocator> &&leaves) :
		m_leaves(leaves), m_mask(0) {

		for (size_t i = 0; i < m_leaves.size(); i++) {
			m_mask |= m_leaves[i]->type_mask();
		}
	}

	inline LeafVectorBase(LeafVectorBase<Allocator> &&v) :
		m_leaves(std::move(v.m_leaves)), m_mask(v.m_mask) {
	}

	inline void push_back(BaseExpressionRef &&leaf) {
		m_mask |= leaf->type_mask();
		m_leaves.emplace_back(std::move(leaf));
	}

	inline void push_back_copy(const BaseExpressionRef &leaf) {
		push_back(BaseExpressionRef(leaf));
	}

	inline TypeMask type_mask() const {
		return m_mask;
	}

	inline bool empty() const {
		return m_leaves.empty();
	}

	inline size_t size() const {
		return m_leaves.size();
	}

	inline void reserve(size_t n) {
		m_leaves.reserve(n);
	}

	inline auto begin() const {
		return m_leaves.begin();
	}

	inline auto end() const {
		return m_leaves.end();
	}

	inline std::vector<BaseExpressionRef> &&unsafe_grab_internal_vector() {
		return std::move(m_leaves);
	}

	inline BaseExpressionRef &&unsafe_grab_leaf(size_t i) {
		return std::move(m_leaves[i]);
	}

	inline void sort() {
		std::sort(
			MutableIterator(m_leaves.begin()),
			MutableIterator(m_leaves.end()),
			[] (const SwappableBaseExpressionRef &x, const SwappableBaseExpressionRef &y) {
				return x->sort_key().compare(y->sort_key()) < 0;
			});
	}
};

using LeafVector = LeafVectorBase<std::allocator<BaseExpressionRef>>;

class TempVector : public std::vector<UnsafeBaseExpressionRef, VectorAllocator<UnsafeBaseExpressionRef>> {
public:
    inline TempVector() : vector<UnsafeBaseExpressionRef, VectorAllocator<UnsafeBaseExpressionRef>>(
        Pool::unsafe_ref_vector_allocator()) {
    }

    inline ExpressionRef to_list(const Evaluation &evaluation) const;
};

template<typename FReference, typename R>
inline auto generate_vector_sequentially(FReference f, const R &r) {
	LeafVector v;
	r(v);
	auto store = [&v] (BaseExpressionRef &&leaf) mutable {
		v.push_back(std::move(leaf));
	};
	f(store);
	return v;
}

class Generator {
};

class FGenerator : public Generator { // [f]ixed size generator
};

class VGenerator : public Generator { // [v]ariable size generator
};

template<typename FReference>
struct FSGenerator : public FGenerator { // [f]ixed size [s]equential
	const FReference f;
	const size_t n;

	inline FSGenerator(FReference f_, size_t n_) : f(f_), n(n_) {
	}

	inline size_t size() const {
		return n;
	}

	template<size_t N>
	std::tuple<std::array<BaseExpressionRef, N>, TypeMask> array() const {
		assert(n == N);
		std::array<BaseExpressionRef, N> array;
		TypeMask mask = 0;
		size_t i = 0;
		auto store = [&mask, &array, &i] (BaseExpressionRef &&leaf) mutable {
            assert(i < N);
			mask |= leaf->type_mask();
			array[i++].unsafe_mutate(std::move(leaf));
		};
		f(store);
        assert(i == N);
		return std::make_tuple(std::move(array), mask);
	}

	LeafVector vector() const {
		return generate_vector_sequentially(f, [this] (auto &v) {
			v.reserve(n);
		});
	}
};

template<typename FReference>
struct VSGenerator : public VGenerator { // [v]ariable size [s]equential
	const FReference f;

	inline VSGenerator(FReference f_) : f(f_) {
	}

	LeafVector vector() const {
		return generate_vector_sequentially(f, [] (auto &v) {
		});
	}
};

#if FASTER_COMPILE
using GenericSGeneratorCallback = std::function<void(const std::function<void(BaseExpressionRef&&)>&)>;
#endif

template<typename F>
inline auto sequential(const F &f, size_t n) {
#if FASTER_COMPILE
	return FSGenerator<GenericSGeneratorCallback>(f, n);
#else
	return FSGenerator<const F&>(f, n);
#endif
}

template<typename F>
inline auto sequential(F &f, size_t n) {
#if FASTER_COMPILE
	return FSGenerator<GenericSGeneratorCallback>(f, n);
#else
	return FSGenerator<F&>(f, n);
#endif
}

template<typename F>
inline auto sequential(const F &f) {
#if FASTER_COMPILE
	return VSGenerator<GenericSGeneratorCallback>(f);
#else
	return VSGenerator<const F&>(f);
#endif
}

template<typename F>
inline auto sequential(F &f) {
#if FASTER_COMPILE
	return VSGenerator<GenericSGeneratorCallback>(f);
#else
	return VSGenerator<F&>(f);
#endif
}

template<typename F>
struct FPGenerator : public FGenerator { // [f]ixed size [p]arallel
private:
	const F &m_generate;
	const size_t n;

public:
	inline FPGenerator(const F &f_, size_t n_) : m_generate(f_), n(n_) {
	}

	inline size_t size() const {
		return n;
	}

	template<size_t N>
	std::tuple<std::array<BaseExpressionRef, N>, TypeMask> array() const {
		std::array<BaseExpressionRef, N> array;
		std::atomic<TypeMask> mask;
		mask.store(0, std::memory_order_relaxed);
		const F &f = m_generate;
		parallelize([&array, &f, &mask] (size_t i) {
			BaseExpressionRef leaf = f(i);
			mask.fetch_or(leaf->type_mask(), std::memory_order_relaxed);
			array[i].unsafe_mutate(std::move(leaf));
		}, N);
		return std::make_tuple(std::move(array), mask.load(std::memory_order_relaxed));
	}

	LeafVector vector() const {
		std::vector<BaseExpressionRef> v(n);
		std::atomic<TypeMask> mask;
		mask.store(0, std::memory_order_relaxed);
		const F &f = m_generate;
		parallelize([&v, &f, &mask] (size_t i) {
			BaseExpressionRef leaf = f(i);
			mask.fetch_or(leaf->type_mask(), std::memory_order_relaxed);
			v[i].unsafe_mutate(std::move(leaf));
		}, n);
		return LeafVector(std::move(v), mask.load(std::memory_order_relaxed));
	}
};

template<typename F>
struct VPGenerator : public VGenerator { // [v]ariable size [p]arallel
private:
	const F &m_generate;
	const size_t n;

public:
	inline VPGenerator(const F &f_, size_t n_) : m_generate(f_), n(n_) {
	}

	LeafVector vector() const {
		std::vector<BaseExpressionRef> v;
		v.reserve(n); // allocate for worst case
		std::atomic<TypeMask> mask;
		mask.store(0, std::memory_order_relaxed);
		const F &f = m_generate;
		parallelize([&v, &f, &mask] (size_t i) {
			BaseExpressionRef leaf = f(i);
			if (leaf) {
				mask.fetch_or(leaf->type_mask(), std::memory_order_relaxed);
				v.emplace_back(std::move(leaf)); // FIXME
			}
		}, n);
		return LeafVector(std::move(v), mask.load(std::memory_order_relaxed));
	}
};

#if FASTER_COMPILE
using GenericPGeneratorCallback = std::function<BaseExpressionRef(size_t)>;
#endif

template<typename F>
inline auto parallel(const F &f, size_t n) {
#if FASTER_COMPILE
	return FPGenerator<GenericPGeneratorCallback>(f, n);
#else
	return FPGenerator<F>(f, n);
#endif
}

class up_to {
private:
	const size_t m_n;

public:
	inline up_to(size_t n) : m_n(n) {
	}

	inline size_t operator*() const {
		return m_n;
	}
};

template<typename F>
inline auto parallel(const F &f, up_to n) {
#if FASTER_COMPILE
	return VPGenerator<GenericPGeneratorCallback>(f, *n);
#else
	return VPGenerator<F>(f, *n);
#endif
}

#endif //CMATHICS_GENERATOR_H
