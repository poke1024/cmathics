#pragma once

#include <array>

template<int N, bool valid>
struct TinySlicePart {
};

template<int N>
struct TinySlicePart<N, true> {
    using type = TinySlice<N>;
};

template<int N>
struct TinySlicePart<N, false> {
    using type = TinySlice<0>;
};

template<int N>
class TinySlice : protected std::array<BaseExpressionRef, N>, public BaseRefsSlice<tiny_slice_code(N)> {
private:
    static_assert(N <= MaxTinySliceSize, "N must not be greater than MaxTinySliceSize");

	typedef BaseRefsSlice<tiny_slice_code(N)> BaseSlice;

	typedef std::array<BaseExpressionRef, N> Array;

public:
	inline const BaseExpressionRef *begin() const {
		return Array::data();
	}

	inline const BaseExpressionRef *end() const {
		return Array::data() + N;
	}

	inline const BaseExpressionRef &operator[](size_t i) const {
		return Array::data()[i];
	}

	constexpr size_t size() const {
		return N;
	}

public:
	template<typename V>
	using PrimitiveCollection = FixedSizePointerCollection<N, BaseExpressionRef, BaseExpressionToPrimitive<V>>;

	using LeafCollection = FixedSizePointerCollection<N, BaseExpressionRef>;

	inline LeafCollection leaves() const {
		return LeafCollection(begin());
	}

	template<typename V>
	inline PrimitiveCollection<V> primitives() const {
		return PrimitiveCollection<V>(begin(), BaseExpressionToPrimitive<V>());
	}

	inline TinySlice(std::tuple<Array, TypeMask> &&arguments) :
		Array(std::move(std::get<0>(arguments))),
		BaseSlice(Array::data(), N, std::get<1>(arguments)) {
	}

public:
	inline TinySlice() :
		BaseSlice(Array::data(), N, N == 0 ? 0 : UnknownTypeMask) {
	}

    inline TinySlice(const TinySlice<N> &slice) :
		BaseSlice(Array::data(), N, slice.m_type_mask) {
        // mighty important to provide a copy iterator so that _begin
	    // won't get copied from other slice.
	    for (size_t i = 0; i < N; i++) {
		    Array::data()[i].unsafe_mutate(BaseExpressionRef(slice[i]));
	    }
    }

	template<typename F>
	inline TinySlice(const FSGenerator<F> &g) :
		TinySlice(g.template array<N>()) {
	}

	template<typename F>
	inline TinySlice(const FPGenerator<F> &g) :
		TinySlice(g.template array<N>()) {
	}

    inline TinySlice(
	    const std::vector<BaseExpressionRef> &refs,
	    TypeMask type_mask = UnknownTypeMask) :

	    BaseSlice(Array::data(), N, type_mask) {
        assert(refs.size() == N);
	    for (size_t i = 0; i < N; i++) {
		    Array::data()[i].unsafe_mutate(BaseExpressionRef(refs[i]));
	    }
    }

	inline TinySlice(
		const std::initializer_list<BaseExpressionRef> &refs,
		TypeMask type_mask = UnknownTypeMask) :

		BaseSlice(Array::data(), N, type_mask) {
		assert(refs.size() == N);
		size_t i = 0;
		for (const BaseExpressionRef &ref : refs) {
			Array::data()[i++].unsafe_mutate(BaseExpressionRef(ref));
		}
	}

    inline TinySlice(
	    const BaseExpressionRef *refs,
        TypeMask type_mask = UnknownTypeMask) :

	    BaseSlice(Array::data(), N, type_mask) {

	    for (size_t i = 0; i < N; i++) {
		    Array::data()[i].unsafe_mutate(BaseExpressionRef(refs[i]));
	    }
    }

	inline explicit TinySlice(Array &&array) :
		Array(array), BaseSlice(Array::data(), N, N == 0 ? 0 : UnknownTypeMask) {

	}

	template<typename F>
	static inline TinySlice create(F &f, size_t n) {
		assert(n == N);
		return StaticSlice(sequential(f, n));
	}

	template<typename F>
	static inline TinySlice parallel_create(const F &f, size_t n) {
		assert(n == N);
		return StaticSlice(parallel(f, n));
	}

	template<typename F>
	inline TinySlice map(const F &f) const {
		const auto &slice = *this;
		return TinySlice(sequential([&f, &slice] (auto &store) {
			for (size_t i = 0; i < N; i++) {
				store(f(slice[i]));
			}
		}, N));
	}

	template<typename F>
	inline TinySlice parallel_map(const F &f) const {
		const auto &slice = *this;
		return TinySlice(parallel([&f, &slice] (size_t i) {
			return f(slice[i]);
		}, N));
	}

	inline TinySlice slice(size_t begin, size_t end) const {
		throw std::runtime_error("cannot dynamically slice a TinySlice");
	}

    template<int M>
    using Rest = typename TinySlicePart<N - M, N - M >= 0>::type;

    template<int M>
	inline Rest<M> drop() const {
        return Rest<M>(Array::data() + M);
	}

	inline auto late_init() {
		return std::make_tuple(Array::data(), &this->m_type_mask);
	}

	inline bool is_packed() const {
		return false;
	}

	inline TinySlice<N> unpack() const {
		return *this;
	}

	inline const BaseExpressionRef *refs() const {
		return Array::data();
	}

	inline BaseExpressionRef leaf(size_t i) const {
		return Array::data()[i];
	}
};

typedef TinySlice<0> EmptySlice;
