//
// Created by Bernhard on 23.10.16.
//

#ifndef CMATHICS_LEAVES_H
#define CMATHICS_LEAVES_H

#include <string>
#include <stdint.h>
#include <functional>
#include <assert.h>
#include <climits>
#include <vector>
#include <experimental/optional>

#include "primitives.h"
#include "string.h"
#include "promote.h"

typedef std::experimental::optional<TypeMask> OptionalTypeMask;

template<typename T>
inline TypeMask calc_type_mask(const T &container) {
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
class PrimitiveToBaseExpression {
public:
	inline BaseExpressionRef convert(const T &u) const {
		return from_primitive(u);
	}
};

template<typename T>
class BaseExpressionToPrimitive {
public:
	inline T convert(const BaseExpressionRef &u) const {
		return to_primitive<T>(u);
	}
};

template<typename V>
class PromotePrimitive {
public:
    template<typename U>
    inline V convert(const U &x) const {
        return promote<U, V>(x);
    }
};

/*template<>
class PrimitiveToPrimitive<mpint> {
public:
	inline mpint convert(const mpz_class &x) const {
		return mpint(x);
	}

	inline mpint convert(machine_integer_t x) const {
		return mpint(x);
	}

	inline mpint convert(const mpq_class &x) const {
		throw std::runtime_error("unsupported conversion");
	}

	inline mpint convert(const std::string &x) const {
		throw std::runtime_error("unsupported conversion");
	}
};*/

/*template<>
class PrimitiveToPrimitive<machine_real_t> {
public:
	inline machine_real_t convert(machine_real_t x) const {
		return x;
	}

	inline machine_real_t convert(const std::string &x) const {
		throw std::runtime_error("unsupported conversion");
	}

	inline machine_real_t convert(const mpq_class &x) const {
		throw std::runtime_error("unsupported conversion");
	}

	inline machine_real_t convert(const mpz_class &x) const {
		throw std::runtime_error("unsupported conversion");
	}
};*/

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

template<typename Size, SliceTypeId _type_id>
class Slice {
protected:
	const Size _size;

public:
	static constexpr SliceTypeId type_id = _type_id;

	inline Slice(size_t size) : _size(size) {
		assert(static_cast<Size>(size) == size);
	}

	inline size_t size() const {
		return _size;
	}

	/*inline constexpr SliceTypeId type_id() const {
		return _type_id;
	}*/
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
struct PackSliceTypeId {
};

template<>
struct PackSliceTypeId<machine_integer_t> {
	static const SliceTypeId id = PackSliceMachineIntegerCode;
};

template<>
struct PackSliceTypeId<machine_real_t> {
	static const SliceTypeId id = PackSliceMachineRealCode;
};

template<>
struct PackSliceTypeId<mpz_class> {
	static const SliceTypeId id = PackSliceBigIntegerCode;
};

template<>
struct PackSliceTypeId<mpq_class> {
	static const SliceTypeId id = PackSliceRationalCode;
};

template<>
struct PackSliceTypeId<std::string> {
	static const SliceTypeId id = PackSliceStringCode;
};

template<typename U>
class PackSlice : public Slice<size_t, PackSliceTypeId<U>::id> {
private:
	typename PackExtent<U>::Ref _extent;
	const U * const _begin;

public:
	template<typename V>
	using PrimitiveCollection = PointerCollection<U, PromotePrimitive<V>>;

	using LeafCollection = PointerCollection<U, PrimitiveToBaseExpression<U>>;

	using BaseSlice = Slice<size_t, PackSliceTypeId<U>::id>;

public:
	inline PackSlice(const std::vector<U> &data) :
		_extent(std::make_shared<PackExtent<U>>(data)),
		_begin(_extent->address()),
		BaseSlice(data.size()) {
	}

	inline PackSlice(std::vector<U> &&data) :
		_extent(std::make_shared<PackExtent<U>>(data)),
		_begin(_extent->address()),
		BaseSlice(data.size()) {
	}

	inline PackSlice(const typename PackExtent<U>::Ref &extent, const U *begin, size_t size) :
		_extent(extent),
		_begin(begin),
		BaseSlice(size) {
	}

    inline constexpr TypeMask type_mask() const {
	    // constexpr is important here, as it allows apply() to optimize this for most cases that
	    // need specific type masks (e.g. optimize evaluate of leaves on PackSlices to a noop).
        return MakeTypeMask(TypeFromPrimitive<U>::type);
    }

	template<typename V>
	PrimitiveCollection<V> primitives() const {
		return PrimitiveCollection<V>(_begin, BaseSlice::_size, PromotePrimitive<V>());
	}

	LeafCollection leaves() const {
		return LeafCollection(_begin, BaseSlice::_size, PrimitiveToBaseExpression<U>());
	}

	inline BaseExpressionRef operator[](size_t i) const {
		return from_primitive(_begin[i]);
	}

	PackSlice<U> slice(index_t begin, index_t end = INDEX_MAX) const {
        const size_t size = BaseSlice::_size;

        if (begin < 0) {
            begin = size - (-begin % size);
        }
        if (end < 0) {
            end = size - (-end % size);
        }

		const index_t offset = _begin - _extent->address();
		const index_t n = _extent->size();
		assert(offset <= n);

		end = std::min(end, n - offset);
		begin = std::min(begin, end);

        if (end <= begin) {
            return PackSlice<U>(_extent, _begin, 0);
        }

		return PackSlice<U>(_extent, _begin + begin, end - begin);
	}

	inline bool is_packed() const {
		return true;
	}

	RefsSlice unpack() const;

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

template<typename Size, SliceTypeId _type_id>
class BaseRefsSlice : public Slice<Size, _type_id> {
protected:
    const BaseExpressionRef * const _begin;
    mutable OptionalTypeMask _type_mask;

public:
    template<typename V>
    using PrimitiveCollection = PointerCollection<BaseExpressionRef, BaseExpressionToPrimitive<V>>;

public:
    using LeafCollection = PointerCollection<BaseExpressionRef, PassBaseExpression>;

public:
    template<typename V>
    inline PrimitiveCollection<V> primitives() const {
        return PrimitiveCollection<V>(_begin, Slice<Size, _type_id>::size(), BaseExpressionToPrimitive<V>());
    }

    inline LeafCollection leaves() const {
        return LeafCollection(_begin, Slice<Size, _type_id>::size(), PassBaseExpression());
    }

	inline OptionalTypeMask sliced_type_mask(size_t new_size) const {
		if (new_size == 0) {
			return OptionalTypeMask(0);
		} else if (_type_mask) {
			if (is_homogenous(*_type_mask)) {
				return _type_mask;
			} else {
				return OptionalTypeMask();
			}
		} else {
			return OptionalTypeMask();
		}
	}

    inline TypeMask type_mask() const {
        if (_type_mask) {
	        return *_type_mask;
        } else {
	        const BaseExpressionRef *p = _begin;
	        const BaseExpressionRef *p_end = p + Slice<Size, _type_id>::size();
	        TypeMask mask = 0;
	        while (p != p_end) {
		        mask |= (*p++)->type_mask();
	        }
	        _type_mask = mask;
	        return mask;
        }
    }

public:
    inline BaseRefsSlice(const BaseExpressionRef *begin, size_t size, OptionalTypeMask type_mask) :
        _begin(begin), Slice<Size, _type_id>(size), _type_mask(type_mask) {
    }

    inline const BaseExpressionRef *begin() const {
        return _begin;
    }

    inline const BaseExpressionRef *end() const {
        return _begin + Slice<Size, _type_id>::size();
    }

    inline const BaseExpressionRef &operator[](size_t i) const {
        return _begin[i];
    }
};

class RefsSlice : public BaseRefsSlice<size_t, SliceTypeId::RefsSliceCode> {
private:
	typename RefsExtent::Ref _extent;

    inline RefsSlice(const typename RefsExtent::Ref &extent, OptionalTypeMask type_mask) :
        BaseRefsSlice(extent->address(), extent->size(), type_mask),
        _extent(extent) {
    }

public:
    inline RefsSlice(const RefsSlice &slice) :
        BaseRefsSlice(slice._begin, slice._size, slice._type_mask),
        _extent(slice._extent) {
	}

	inline RefsSlice() : BaseRefsSlice(nullptr, 0, 0) {
	}

    inline RefsSlice(const std::vector<BaseExpressionRef> &data, OptionalTypeMask type_mask) :
        RefsSlice(std::make_shared<RefsExtent>(data), type_mask) {
	}

	inline RefsSlice(std::vector<BaseExpressionRef> &&data, OptionalTypeMask type_mask) :
        RefsSlice(std::make_shared<RefsExtent>(data), type_mask) {
	}

	inline RefsSlice(const std::initializer_list<BaseExpressionRef> &data, OptionalTypeMask type_mask) :
        RefsSlice(std::make_shared<RefsExtent>(data), type_mask) {
	}

	inline RefsSlice(
        const typename RefsExtent::Ref &extent,
		const BaseExpressionRef * const begin,
        const BaseExpressionRef * const end,
        OptionalTypeMask type_mask) :
        BaseRefsSlice(begin, end - begin, type_mask),
		_extent(extent) {
	}

	inline RefsSlice slice(index_t begin, index_t end = INDEX_MAX) const {
        const size_t size = _size;

        if (begin < 0) {
            begin = size - (-begin % size);
        }
        if (end < 0) {
            end = size - (-end % size);
        }

		const index_t offset = _begin - _extent->address();
		const index_t n = _extent->size();
		assert(offset <= n);

		end = std::min(end, n - offset);
		begin = std::min(begin, end);

        if (end <= begin) {
            return RefsSlice();
        } else if (begin == 0 && end == size) {
			return *this;
		} else {
	        return RefsSlice(_extent, _begin + begin, _begin + end, sliced_type_mask(end - begin));
        }
	}

	inline bool is_packed() const {
		return false;
	}

	inline RefsSlice unpack() const {
		return *this;
	}

	inline const BaseExpressionRef *refs() const {
		return _begin;
	}
};

template<size_t N>
class InPlaceRefsSlice : public BaseRefsSlice<uint8_t, in_place_slice_type_id(N)> {
private:
    mutable BaseExpressionRef _refs[N];

	typedef BaseRefsSlice<uint8_t, in_place_slice_type_id(N)> BaseSlice;

public:
	inline InPlaceRefsSlice() :
		BaseSlice(&_refs[0], N, N == 0 ? OptionalTypeMask(0) : OptionalTypeMask()) {
	}

    inline InPlaceRefsSlice(const InPlaceRefsSlice<N> &slice) :
		BaseSlice(&_refs[0], slice.size(), slice._type_mask) {
        // mighty important to provide a copy iterator so that _begin won't get copied from other slice.
        std::copy(slice._refs, slice._refs + slice.size(), _refs);
    }

    inline InPlaceRefsSlice(const std::vector<BaseExpressionRef> &refs, OptionalTypeMask type_mask) :
		BaseSlice(&_refs[0], N, type_mask) {
        assert(refs.size() == N);
        std::copy(refs.begin(), refs.end(), _refs);
    }

	inline InPlaceRefsSlice(const std::initializer_list<BaseExpressionRef> &refs, OptionalTypeMask type_mask) :
		BaseSlice(&_refs[0], N, type_mask) {
		assert(refs.size() == N);
		std::copy(refs.begin(), refs.end(), _refs);
	}

    inline InPlaceRefsSlice(const BaseExpressionRef *refs, size_t size, OptionalTypeMask type_mask) :
		BaseSlice(&_refs[0], size, type_mask) {
        assert(size <= N);
        std::copy(refs, refs + size, _refs);
    }

	BaseExpressionRef *late_init() const {
		return &_refs[0];
	}

    inline InPlaceRefsSlice<N> slice(index_t begin, index_t end = INDEX_MAX) const {
        const size_t size = BaseSlice::size();

        if (begin < 0) {
            begin = size - (-begin % size);
        }
        if (end < 0) {
            end = size - (-end % size);
        }

        end = std::min(end, (index_t)size);
        begin = std::min(begin, end);

        if (end <= begin) {
            return InPlaceRefsSlice<N>(nullptr, 0, 0);
        } else if (begin == 0 && end == size) {
	        return *this;
        } else {
            return InPlaceRefsSlice<N>(&_refs[begin], end - begin, BaseSlice::sliced_type_mask(end - begin));
        }
    }

	inline bool is_packed() const {
		return false;
	}

	inline InPlaceRefsSlice<N> unpack() const {
		return *this;
	}

	inline const BaseExpressionRef *refs() const {
		return &_refs[0];
	}
};

typedef InPlaceRefsSlice<0> EmptySlice;

template<typename U>
inline RefsSlice PackSlice<U>::unpack() const {
	std::vector<BaseExpressionRef> leaves;
	for (auto leaf : this->leaves()) {
		leaves.push_back(leaf);
	}
	return RefsSlice(std::move(leaves), type_mask());
}


#endif //CMATHICS_LEAVES_H
