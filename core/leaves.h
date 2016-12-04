#ifndef CMATHICS_LEAVES_H
#define CMATHICS_LEAVES_H

#include <string>
#include <stdint.h>
#include <functional>
#include <assert.h>
#include <climits>
#include <vector>

#include "primitives.h"
#include "string.h"

template<typename T>
inline TypeMask exact_type_mask(const T &container) {
	TypeMask mask = 0;
	for (auto leaf : container) {
		mask |= leaf->base_type_mask();
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
class PackExtent {
private:
	const std::vector<U> _data;

public:
	typedef std::shared_ptr<PackExtent<U>> Ref;

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

	inline PackedSlice(const std::vector<U> &data) :
		_extent(std::make_shared<PackExtent<U>>(data)),
		_begin(_extent->address()),
		BaseSlice(nullptr, data.size()) {
		assert(data.size() >= MinPackedSliceSize);
	}

	inline PackedSlice(std::vector<U> &&data) :
		_extent(std::make_shared<PackExtent<U>>(data)),
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

	inline PackedSlice<U> slice(size_t begin, size_t end) const {
		assert(end - begin >= MinPackedSliceSize);
		return PackedSlice<U>(_extent, _begin + begin, end - begin);
	}

    inline constexpr TypeMask type_mask() const {
	    // constexpr is important here, as it allows apply() to optimize this for most cases that
	    // need specific type masks (e.g. optimize evaluate of leaves on PackSlices to a noop).
        return MakeTypeMask(TypeFromPrimitive<U>::type);
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

class RefsExtent {
private:
	const std::vector<BaseExpressionRef> _data;

public:
	typedef std::shared_ptr<RefsExtent> Ref;

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

template<SliceCode _code>
class BaseRefsSlice : public TypedSlice<_code> {
public:
    mutable TypeMask _type_mask;

public:
	inline TypeMask sliced_type_mask(size_t new_size) const {
		if (new_size == 0) {
			return 0;
		} else if (is_exact_type_mask(_type_mask)) {
			if (is_homogenous(_type_mask)) {
				return _type_mask;
			} else {
				return _type_mask | TypeMaskIsInexact;
			}
		} else {
			return _type_mask;
		}
	}

	inline TypeMask type_mask() const {
		return _type_mask;
	}

	inline TypeMask exact_type_mask() const {
        if (is_exact_type_mask(_type_mask)) {
	        return _type_mask;
        } else {
	        const BaseExpressionRef *p = Slice::_address;
	        const BaseExpressionRef *p_end = p + Slice::_size;
	        TypeMask mask = 0;
	        while (p != p_end) {
		        mask |= (*p++)->base_type_mask();
	        }
	        _type_mask = mask;
	        return mask;
        }
    }

	inline void init_type_mask(TypeMask type_mask) const {
		_type_mask = type_mask;
	}

public:
    inline BaseRefsSlice(const BaseExpressionRef *address, size_t size, TypeMask type_mask) :
	    TypedSlice<_code>(address, size), _type_mask(type_mask) {
    }

};

class DynamicSlice : public BaseRefsSlice<SliceCode::DynamicSliceCode> {
private:
	typename RefsExtent::Ref _extent;

    inline DynamicSlice(const typename RefsExtent::Ref &extent, TypeMask type_mask) :
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
        BaseRefsSlice(slice.begin(), slice.size(), slice._type_mask),
        _extent(slice._extent) {
	}

	inline DynamicSlice() : BaseRefsSlice(nullptr, 0, 0) {
	}

    inline DynamicSlice(const std::vector<BaseExpressionRef> &data, TypeMask type_mask) :
		DynamicSlice(std::make_shared<RefsExtent>(data), type_mask) {

	    assert(data.size() > MaxStaticSliceSize);
	}

	inline DynamicSlice(std::vector<BaseExpressionRef> &&data, TypeMask type_mask) :
		DynamicSlice(std::make_shared<RefsExtent>(data), type_mask) {

		assert(data.size() > MaxStaticSliceSize);
	}

	inline DynamicSlice(const std::initializer_list<BaseExpressionRef> &data, TypeMask type_mask) :
		DynamicSlice(std::make_shared<RefsExtent>(data), type_mask) {

		assert(data.size() > MaxStaticSliceSize);
	}

	inline DynamicSlice(
        const typename RefsExtent::Ref &extent,
		const BaseExpressionRef * const begin,
        const BaseExpressionRef * const end,
        TypeMask type_mask) :
        BaseRefsSlice(begin, end - begin, type_mask),
		_extent(extent) {

		assert(end - begin > MaxStaticSliceSize);
	}

	inline DynamicSlice slice(size_t begin, size_t end) const {
		return DynamicSlice(_extent, _address + begin, _address + end, sliced_type_mask(end - begin));
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

template<size_t N>
class StaticSlice : public BaseRefsSlice<static_slice_code(N)> {
private:
    mutable BaseExpressionRef _refs[N];

	typedef BaseRefsSlice<static_slice_code(N)> BaseSlice;

public:
	inline const BaseExpressionRef *begin() const {
		return &_refs[0];
	}

	inline const BaseExpressionRef *end() const {
		return &_refs[N];
	}

	inline const BaseExpressionRef &operator[](size_t i) const {
		return _refs[i];
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

public:
	inline StaticSlice() :
		BaseSlice(&_refs[0], N, N == 0 ? 0 : UnknownTypeMask) {
	}

    inline StaticSlice(const StaticSlice<N> &slice) :
		BaseSlice(&_refs[0], N, slice._type_mask) {
        // mighty important to provide a copy iterator so that _begin won't get copied from other slice.
        std::copy(slice._refs, slice._refs + N, _refs);
    }

    inline StaticSlice(
	    const std::vector<BaseExpressionRef> &refs,
	    TypeMask type_mask = UnknownTypeMask) :

	    BaseSlice(&_refs[0], N, type_mask) {
        assert(refs.size() == N);
        std::copy(refs.begin(), refs.end(), _refs);
    }

	inline StaticSlice(
		const std::initializer_list<BaseExpressionRef> &refs,
		TypeMask type_mask = UnknownTypeMask) :

		BaseSlice(&_refs[0], N, type_mask) {
		assert(refs.size() == N);
		std::copy(refs.begin(), refs.end(), _refs);
	}

    inline StaticSlice(
	    const BaseExpressionRef *refs,
        TypeMask type_mask = UnknownTypeMask) :

	    BaseSlice(&_refs[0], N, type_mask) {
        std::copy(refs, refs + N, _refs);
    }

	inline StaticSlice slice(size_t begin, size_t end) const {
		throw std::runtime_error("cannot slice a StaticSlice");
	}

	std::tuple<BaseExpressionRef*, TypeMask*> late_init() const {
		return std::make_tuple(&_refs[0], &this->_type_mask);
	}

	inline bool is_packed() const {
		return false;
	}

	inline StaticSlice<N> unpack() const {
		return *this;
	}

	inline const BaseExpressionRef *refs() const {
		return &_refs[0];
	}

	inline BaseExpressionRef leaf(size_t i) const {
		return _refs[i];
	}
};

typedef StaticSlice<0> EmptySlice;

template<typename U>
inline DynamicSlice PackedSlice<U>::unpack() const {
	std::vector<BaseExpressionRef> leaves;
	for (auto leaf : this->leaves()) {
		leaves.push_back(leaf);
	}
	return DynamicSlice(std::move(leaves), type_mask());
}

#endif //CMATHICS_LEAVES_H
