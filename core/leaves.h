#ifndef CMATHICS_LEAVES_H
#define CMATHICS_LEAVES_H

#include <string>
#include <stdint.h>
#include <functional>
#include <assert.h>
#include <climits>
#include <vector>
#include <array>

#include "primitives.h"
#include "string.h"
#include "generator.h"

template<typename T>
inline TypeMask exact_type_mask(const T &container) {
	TypeMask mask = 0;
	for (auto leaf : container) {
		mask |= leaf->type_mask();
	}
	return mask;
}

class PassBaseExpression {
public:
	inline BaseExpressionRef convert(const BaseExpressionRef &u) const {
		return u;
	}
};

template<typename T>
class PrimitiveToBaseExpression;

template<typename T>
class BaseExpressionToPrimitive;

template<typename V>
class PromotePrimitive {
public:
    template<typename U>
    inline V convert(const U &x) const {
        return V(x);
    }
};

template<>
class PromotePrimitive<machine_real_t> {
public:
	template<typename U>
	inline machine_real_t convert(const U &x) const {
		return machine_real_t(x);
	}

	inline machine_real_t convert(const std::string &x) const {
		throw std::runtime_error("illegal promotion");
	}

	inline machine_real_t convert(const mpz_class &x) const {
		throw std::runtime_error("illegal promotion");
	}

	inline machine_real_t convert(const mpq_class &x) const {
		throw std::runtime_error("illegal promotion");
	}
};

template<>
class PromotePrimitive<Numeric::Z> {
public:
	template<typename U>
	inline Numeric::Z convert(const U &x) const {
		return Numeric::Z(x);
	}

	inline Numeric::Z convert(const std::string &x) const {
		throw std::runtime_error("illegal promotion");
	}

	inline Numeric::Z convert(const mpq_class &x) const {
		throw std::runtime_error("illegal promotion");
	}
};


template<typename T, typename TypeConverter>
class PointerIterator {
private:
	const T *_ptr;
	const TypeConverter _converter;

public:
	inline PointerIterator(const TypeConverter &converter) : _converter(converter) {
	}

	inline explicit PointerIterator(const TypeConverter &converter, const T *ptr) :
		_converter(converter), _ptr(ptr) {
	}

	inline auto operator*() const {
		return _converter.convert(*_ptr);
	}

	inline bool operator==(const PointerIterator<T, TypeConverter> &other) const {
		return _ptr == other._ptr;
	}

	inline bool operator!=(const PointerIterator<T, TypeConverter> &other) const {
		return _ptr != other._ptr;
	}

	inline PointerIterator<T, TypeConverter> &operator++() {
		_ptr += 1;
		return *this;
	}

	inline PointerIterator<T, TypeConverter> operator++(int) const {
		return PointerIterator<T, TypeConverter>(_ptr + 1);
	}
};

template<typename T, typename TypeConverter>
class PointerCollection {
private:
	const TypeConverter _converter;
	const T * const _data;
	const size_t _size;

public:
	using Iterator = PointerIterator<T, TypeConverter>;

	inline PointerCollection(const T *data, size_t size, const TypeConverter &converter) :
		_converter(converter), _data(data), _size(size) {
	}

	inline Iterator begin() const {
		return PointerIterator<T, TypeConverter>(_converter, _data);
	}

	inline Iterator end() const {
		return PointerIterator<T, TypeConverter>(_converter, _data + _size);
	}

	inline auto operator[](size_t i) const {
		return *Iterator(_converter, _data + i);
	}
};

template<int N, typename T, typename TypeConverter>
class FixedSizePointerCollection {
private:
	const TypeConverter _converter;
	const T * const _data;

public:
	using Iterator = PointerIterator<T, TypeConverter>;

	inline FixedSizePointerCollection(const T *data, const TypeConverter &converter) :
		_converter(converter), _data(data) {
	}

	inline Iterator begin() const {
		return PointerIterator<T, TypeConverter>(_converter, _data);
	}

	inline Iterator end() const {
		return PointerIterator<T, TypeConverter>(_converter, _data + N);
	}

	inline auto operator[](size_t i) const {
		return *Iterator(_converter, _data + i);
	}
};

template<SliceCode _code>
class TypedSlice : public Slice {
public:
	inline TypedSlice(const BaseExpressionRef *address, size_t size) : Slice(address, size) {
	}

	static constexpr SliceCode code() {
		return _code;
	}
};

template<typename U>
class PackExtent : public Shared<PackExtent<U>, SharedHeap> {
private:
	const std::vector<U> _data;

public:
	typedef ConstSharedPtr<PackExtent<U>> Ref;

	inline explicit PackExtent(const std::vector<U> &data) : _data(data) {
	}

	inline explicit PackExtent(std::vector<U> &&data) : _data(data) {
	}

	inline const std::vector<U> &data() const {
		return _data;
	}

	inline const U *address() {
		return &_data[0];
	}

	inline size_t size() const {
		return _data.size();
	}
};

template<typename U>
struct PackedSliceInfo {
};

template<>
struct PackedSliceInfo<machine_integer_t> {
	static constexpr SliceCode code = PackedSliceMachineIntegerCode;
};

template<>
struct PackedSliceInfo<machine_real_t> {
	static constexpr SliceCode code = PackedSliceMachineRealCode;
};

template<typename U>
class PackedSlice : public TypedSlice<PackedSliceInfo<U>::code> {
private:
	typename PackExtent<U>::Ref _extent;
	const U * const _begin;

public:
	template<typename V>
	using PrimitiveCollection = PointerCollection<U, PromotePrimitive<V>>;

	using LeafCollection = PointerCollection<U, PrimitiveToBaseExpression<U>>;

	using BaseSlice = TypedSlice<PackedSliceInfo<U>::code>;

public:
	inline size_t size() const {
		const size_t n = BaseSlice::_size;
		if (n < MinPackedSliceSize) {
			__builtin_unreachable();
		}
		return n;
	}

	template<typename F>
	inline PackedSlice(const F &f, size_t n) {
		throw std::runtime_error("not implemented");
	}

	inline PackedSlice(const std::vector<U> &data) :
		_extent(new PackExtent<U>(data)),
		_begin(_extent->address()),
		BaseSlice(nullptr, data.size()) {
		assert(data.size() >= MinPackedSliceSize);
	}

	inline PackedSlice(std::vector<U> &&data) :
		_extent(new PackExtent<U>(data)),
		_begin(_extent->address()),
		BaseSlice(nullptr, data.size()) {
		assert(data.size() >= MinPackedSliceSize);
	}

	inline PackedSlice(const typename PackExtent<U>::Ref &extent, const U *begin, size_t size) :
		_extent(extent),
		_begin(begin),
		BaseSlice(nullptr, size) {
		assert(size >= MinPackedSliceSize);
	}

	template<typename F>
	static inline PackedSlice create(F &f, size_t n) {
		throw std::runtime_error("not implemented");
	}

	template<typename F>
	static inline PackedSlice parallel_create(const F &f, size_t n) {
		throw std::runtime_error("not implemented");
	}

	template<typename F>
	inline DynamicSlice map(const F &f) const;

	template<typename F>
	inline DynamicSlice parallel_map(const F &f) const;

	inline PackedSlice<U> slice(size_t begin, size_t end) const {
		assert(end - begin >= MinPackedSliceSize);
		return PackedSlice<U>(_extent, _begin + begin, end - begin);
	}

    template<int M>
    inline PackedSlice<U> drop() const {
        return slice(M, size());
    }

    inline constexpr TypeMask type_mask() const {
	    // constexpr is important here, as it allows apply() to optimize this for most cases that
	    // need specific type masks (e.g. optimize evaluate of leaves on PackSlices to a noop).
        return make_type_mask(TypeFromPrimitive<U>::type);
    }

	inline constexpr TypeMask exact_type_mask() const {
		return type_mask();
	}

	inline void init_type_mask(TypeMask type_mask) const {
		// noop
	}

	template<typename V>
	PrimitiveCollection<V> primitives() const {
		return PrimitiveCollection<V>(_begin, size(), PromotePrimitive<V>());
	}

	LeafCollection leaves() const {
		return LeafCollection(_begin, size(), PrimitiveToBaseExpression<U>());
	}

	inline BaseExpressionRef operator[](size_t i) const;

	inline bool is_packed() const {
		return true;
	}

	DynamicSlice unpack() const;

	inline const BaseExpressionRef *refs() const {
		throw std::runtime_error("cannot get refs on PackSlice");
	}
};

class RefsExtent : public Shared<RefsExtent, SharedPool> {
private:
	const std::vector<BaseExpressionRef> _data;

public:
	inline explicit RefsExtent(const std::vector<BaseExpressionRef> &data) : _data(data) {
	}

	inline explicit RefsExtent(std::vector<BaseExpressionRef> &&data) : _data(data) {
	}

	inline explicit RefsExtent(const std::initializer_list<BaseExpressionRef> &data) : _data(data) {
	}

	inline const BaseExpressionRef *address() const {
		return &_data[0];
	}

	inline size_t size() const {
		return _data.size();
	}
};

inline RefsExtentRef Pool::RefsExtent(const std::vector<BaseExpressionRef> &data) {
	return RefsExtentRef(_s_instance->_refs_extents.construct(data));
}

inline RefsExtentRef Pool::RefsExtent(std::vector<BaseExpressionRef> &&data) {
	return RefsExtentRef(_s_instance->_refs_extents.construct(data));
}

inline RefsExtentRef Pool::RefsExtent(const std::initializer_list<BaseExpressionRef> &data) {
	return RefsExtentRef(_s_instance->_refs_extents.construct(data));
}

inline void Pool::release(class RefsExtent *extent) {
	_s_instance->_refs_extents.destroy(extent);
}

template<SliceCode _code>
class BaseRefsSlice : public TypedSlice<_code> {
public:
    mutable std::atomic<TypeMask> m_type_mask;

public:
	inline TypeMask sliced_type_mask(size_t new_size) const {
		if (new_size == 0) {
			return 0;
		} else {
			const TypeMask mask = m_type_mask;

			if (is_exact_type_mask(mask)) {
				if (is_homogenous(mask)) {
					return mask;
				} else {
					return mask | TypeMaskIsInexact;
				}
			} else {
				return mask;
			}
		}
	}

	inline TypeMask type_mask() const {
		return m_type_mask;
	}

	inline TypeMask exact_type_mask() const {
		const TypeMask mask = m_type_mask;

        if (is_exact_type_mask(mask)) {
	        return mask;
        } else {
	        const BaseExpressionRef *p = Slice::_address;
	        const BaseExpressionRef *p_end = p + Slice::_size;

	        TypeMask new_mask = 0;
	        while (p != p_end) {
		        new_mask |= (*p++)->type_mask();
	        }

	        m_type_mask = mask;
	        return new_mask;
        }
    }

	inline void init_type_mask(TypeMask type_mask) const {
		m_type_mask = type_mask;
	}

public:
    inline BaseRefsSlice(const BaseExpressionRef *address, size_t size, TypeMask type_mask) :
	    TypedSlice<_code>(address, size), m_type_mask(type_mask) {
    }

};

class DynamicSlice : public BaseRefsSlice<SliceCode::DynamicSliceCode> {
private:
	RefsExtentRef _extent;

    inline DynamicSlice(const RefsExtentRef &extent, TypeMask type_mask) :
        BaseRefsSlice(extent->address(), extent->size(), type_mask),
        _extent(extent) {
    }

public:
	inline const BaseExpressionRef *begin() const {
		return _address;
	}

	inline const BaseExpressionRef *end() const {
		return _address + _size;
	}

	inline const BaseExpressionRef &operator[](size_t i) const {
		return _address[i];
	}

	inline size_t size() const {
		const size_t n = _size;
		if (n <= MaxStaticSliceSize) {
			__builtin_unreachable();
		}
		return n;
	}

public:
	template<typename V>
	using PrimitiveCollection = PointerCollection<BaseExpressionRef, BaseExpressionToPrimitive<V>>;

	using LeafCollection = PointerCollection<BaseExpressionRef, PassBaseExpression>;

	inline LeafCollection leaves() const {
		return LeafCollection(begin(), size(), PassBaseExpression());
	}

	template<typename V>
	inline PrimitiveCollection<V> primitives() const {
		return PrimitiveCollection<V>(begin(), size(), BaseExpressionToPrimitive<V>());
	}

public:
	inline DynamicSlice(const DynamicSlice &slice) :
        BaseRefsSlice(slice.begin(), slice.size(), slice.m_type_mask),
        _extent(slice._extent) {
	}

	inline DynamicSlice() : BaseRefsSlice(nullptr, 0, 0) {
	}

	inline DynamicSlice(LeafVector &&leaves) :
		DynamicSlice(Pool::RefsExtent(leaves.unsafe_grab_internal_vector()), leaves.type_mask()) {

		assert(leaves.size() > MaxStaticSliceSize);
	}

	template<typename F>
	inline DynamicSlice(const FSGenerator<F> &generator) : DynamicSlice(generator.vector()) {
	}

	template<typename F>
	inline DynamicSlice(const FPGenerator<F> &generator) : DynamicSlice(generator.vector()) {
	}

	inline DynamicSlice(const std::initializer_list<BaseExpressionRef> &data, TypeMask type_mask) :
		DynamicSlice(Pool::RefsExtent(data), type_mask) {

		assert(data.size() > MaxStaticSliceSize);
	}

	inline DynamicSlice(
        const RefsExtentRef &extent,
		const BaseExpressionRef * const begin,
        const BaseExpressionRef * const end,
        TypeMask type_mask) :
        BaseRefsSlice(begin, end - begin, type_mask),
		_extent(extent) {

		assert(end - begin > MaxStaticSliceSize);
	}

	template<typename F>
	static inline DynamicSlice create(F &f, size_t n) {
		return DynamicSlice(sequential(f, n));
	}

	template<typename F>
	static inline DynamicSlice parallel_create(const F &f, size_t n) {
		return DynamicSlice(parallel(f, n));
	}

	template<typename F>
	inline DynamicSlice map(const F &f) const {
		const size_t n = size();
		const auto &slice = *this;
		return DynamicSlice(sequential([n, &f, &slice] (auto &store) {
			for (size_t i = 0; i < n; i++) {
				store(f(slice[i]));
			}
		}, n));
	}

	template<typename F>
	inline DynamicSlice parallel_map(const F &f) const {
		const auto &slice = *this;
		return DynamicSlice(parallel([&f, &slice] (size_t i) {
			return f(slice[i]);
		}, size()));
	}

	inline DynamicSlice slice(size_t begin, size_t end) const {
		return DynamicSlice(_extent, _address + begin, _address + end, sliced_type_mask(end - begin));
	}

    template<int M>
    inline DynamicSlice drop() const {
        return slice(M, size());
    }

	inline bool is_packed() const {
		return false;
	}

	inline DynamicSlice unpack() const {
		return *this;
	}

	inline const BaseExpressionRef *refs() const {
		return begin();
	}
};

template<typename U>
template<typename F>
inline DynamicSlice PackedSlice<U>::map(const F &f) const {
	const size_t n = size();
	const auto &slice = *this;
	return DynamicSlice(sequential([n, &f, &slice] (auto &store) {
		for (size_t i = 0; i < n; i++) {
			store(f(slice[i]));
		}
	}, n));
}

template<typename U>
template<typename F>
inline DynamicSlice PackedSlice<U>::parallel_map(const F &f) const {
	const auto &slice = *this;
	return DynamicSlice(parallel([&f, &slice] (size_t i) {
		return f(slice[i]);
	}, size()));
}

template<int N, bool valid>
struct StaticSlicePart {
};

template<int N>
struct StaticSlicePart<N, true> {
    using type = StaticSlice<N>;
};

template<int N>
struct StaticSlicePart<N, false> {
    using type = StaticSlice<0>;
};

struct create_using_generator {
};


template<int N>
class StaticSlice : protected std::array<BaseExpressionRef, N>, public BaseRefsSlice<static_slice_code(N)> {
private:
    static_assert(N <= MaxStaticSliceSize, "N must not be greater than MaxStaticSliceSize");

	typedef BaseRefsSlice<static_slice_code(N)> BaseSlice;

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

	using LeafCollection = FixedSizePointerCollection<N, BaseExpressionRef, PassBaseExpression>;

	inline LeafCollection leaves() const {
		return LeafCollection(begin(), PassBaseExpression());
	}

	template<typename V>
	inline PrimitiveCollection<V> primitives() const {
		return PrimitiveCollection<V>(begin(), BaseExpressionToPrimitive<V>());
	}

	inline StaticSlice(std::tuple<Array, TypeMask> &&arguments) :
		Array(std::move(std::get<0>(arguments))),
		BaseSlice(Array::data(), N, std::get<1>(arguments)) {
	}

public:
	inline StaticSlice() :
		BaseSlice(Array::data(), N, N == 0 ? 0 : UnknownTypeMask) {
	}

    inline StaticSlice(const StaticSlice<N> &slice) :
		BaseSlice(Array::data(), N, slice.m_type_mask) {
        // mighty important to provide a copy iterator so that _begin
	    // won't get copied from other slice.
	    for (size_t i = 0; i < N; i++) {
		    Array::data()[i].unsafe_mutate(BaseExpressionRef(slice[i]));
	    }
    }

	template<typename F>
	inline StaticSlice(const FSGenerator<F> &g) :
		StaticSlice(g.template array<N>()) {
	}

	template<typename F>
	inline StaticSlice(const FPGenerator<F> &g) :
		StaticSlice(g.template array<N>()) {
	}

    inline StaticSlice(
	    const std::vector<BaseExpressionRef> &refs,
	    TypeMask type_mask = UnknownTypeMask) :

	    BaseSlice(Array::data(), N, type_mask) {
        assert(refs.size() == N);
	    for (size_t i = 0; i < N; i++) {
		    Array::data()[i].unsafe_mutate(BaseExpressionRef(refs[i]));
	    }
    }

	inline StaticSlice(
		const std::initializer_list<BaseExpressionRef> &refs,
		TypeMask type_mask = UnknownTypeMask) :

		BaseSlice(Array::data(), N, type_mask) {
		assert(refs.size() == N);
		size_t i = 0;
		for (const BaseExpressionRef &ref : refs) {
			Array::data()[i++].unsafe_mutate(BaseExpressionRef(ref));
		}
	}

    inline StaticSlice(
	    const BaseExpressionRef *refs,
        TypeMask type_mask = UnknownTypeMask) :

	    BaseSlice(Array::data(), N, type_mask) {

	    for (size_t i = 0; i < N; i++) {
		    Array::data()[i].unsafe_mutate(BaseExpressionRef(refs[i]));
	    }
    }

	inline explicit StaticSlice(Array &&array) :
		Array(array), BaseSlice(Array::data(), N, N == 0 ? 0 : UnknownTypeMask) {

	}

	template<typename F>
	static inline StaticSlice create(F &f, size_t n) {
		assert(n == N);
		return StaticSlice(sequential(f, n));
	}

	template<typename F>
	static inline StaticSlice parallel_create(const F &f, size_t n) {
		assert(n == N);
		return StaticSlice(parallel(f, n));
	}

	template<typename F>
	inline StaticSlice map(const F &f) const {
		const auto &slice = *this;
		return StaticSlice(sequential([&f, &slice] (auto &store) {
			for (size_t i = 0; i < N; i++) {
				store(f(slice[i]));
			}
		}, N));
	}

	template<typename F>
	inline StaticSlice parallel_map(const F &f) const {
		const auto &slice = *this;
		return StaticSlice(parallel([&f, &slice] (size_t i) {
			return f(slice[i]);
		}, N));
	}

	inline StaticSlice slice(size_t begin, size_t end) const {
		throw std::runtime_error("cannot dynamically slice a StaticSlice");
	}

    template<int M>
    using Rest = typename StaticSlicePart<N - M, N - M >= 0>::type;

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

	inline StaticSlice<N> unpack() const {
		return *this;
	}

	inline const BaseExpressionRef *refs() const {
		return Array::data();
	}

	inline BaseExpressionRef leaf(size_t i) const {
		return Array::data()[i];
	}
};

typedef StaticSlice<0> EmptySlice;

template<typename U>
inline DynamicSlice PackedSlice<U>::unpack() const {
	LeafVector leaves;
	for (auto leaf : this->leaves()) {
		leaves.push_back_copy(leaf);
	}
	return DynamicSlice(std::move(leaves));
}

#endif //CMATHICS_LEAVES_H
