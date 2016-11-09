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
#include <array>

#include "primitives.h"
#include "string.h"

class Match;

template<typename U, typename I>
class PointerCollection {
private:
	const U * const _data;
	const size_t _size;

public:
	inline PointerCollection(const U *data, size_t size) : _data(data), _size(size) {
	}

	inline I begin() const {
		return I(_data);
	}

	inline I end() const {
		return I(_data + _size);
	}

	inline auto operator[](size_t i) const {
		return *I(_data + i);
	}
};

template<typename U, typename V>
class Conversions {
public:
    static inline V convert(const U &u) {
        return V(u);
    }
};

template<>
class Conversions<machine_integer_t, mpq_class> {
public:
    static inline mpq_class convert(const machine_integer_t &u) {
        mpq_class v;
        v = (signed long int)u;
        return v;
    }
};

template<>
class Conversions<machine_integer_t, mpz_class> {
public:
    static inline mpz_class convert(const machine_integer_t &u) {
        mpz_class v;
        v = (signed long int)u;
        return v;
    }
};


template<typename T, typename F>
class PointerIterator {
private:
	const T *_ptr;

public:
	inline PointerIterator() {
	}

	inline explicit PointerIterator(const T *ptr) : _ptr(ptr) {
	}

	inline auto operator*() const {
		return Conversions<T, F>::convert(*_ptr);
	}

	inline bool operator==(const PointerIterator<T, F> &other) const {
		return _ptr == other._ptr;
	}

	inline bool operator!=(const PointerIterator<T, F> &other) const {
		return _ptr != other._ptr;
	}

	inline PointerIterator<T, F> &operator++() {
		_ptr += 1;
		return *this;
	}

	inline PointerIterator<T, F> operator++(int) const {
		return PointerIterator<T, F>(_ptr + 1);
	}
};

class Slice {
protected:
	const size_t _size;

public:
	inline Slice(size_t size) : _size(size) {
	}

	inline size_t size() const {
		return _size;
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

	inline const U *address() {
		return &_data[0];
	}

	inline size_t size() const {
		return _data.size();
	}
};

template<typename U>
class PackSlice : public Slice {
private:
	typename PackExtent<U>::Ref _extent;
	const U * const _begin;

public:
	template<typename V>
	using PrimitiveIterator = PointerIterator<U, V>;

	template<typename V>
	using PrimitiveCollection = PointerCollection<U, PrimitiveIterator<V>>;

public:
	class leaf {
	private:
		const U &_u;
	public:
		inline leaf(const U &u) : _u(u) {
		}

		inline operator BaseExpressionRef() const {
			return from_primitive(_u);
		}

		inline BaseExpressionRef operator->() const {
			return from_primitive(_u);
		}
	};

	using LeafIterator = PointerIterator<U, leaf>;

	using LeafCollection = PointerCollection<U, LeafIterator>;

public:
	inline PackSlice(const std::vector<U> &data) :
		_extent(std::make_shared<PackExtent<U>>(data)), _begin(_extent->address()), Slice(data.size()) {
	}

	inline PackSlice(std::vector<U> &&data) :
		_extent(std::make_shared<PackExtent<U>>(data)), _begin(_extent->address()), Slice(data.size()) {
	}

	inline PackSlice(const typename PackExtent<U>::Ref &extent, const U *begin, size_t size) :
		_extent(extent), _begin(begin), Slice(size) {
	}

    inline TypeMask type_mask() const {
        return 1L << TypeFromPrimitive<U>::type;
    }

    template<typename V>
	PrimitiveCollection<V> primitives() const {
		return PrimitiveCollection<V>(_begin, _size);
	}

	LeafCollection leaves() const {
		return LeafCollection(_begin, _size);
	}

	inline LeafIterator begin() const {
		return LeafIterator(_begin);
	}

	inline LeafIterator end() const {
		return LeafIterator(_begin + _size);
	}

	inline BaseExpressionRef operator[](size_t i) const {
		return from_primitive(_begin[i]);
	}

	PackSlice<U> slice(index_t begin, index_t end = INDEX_MAX) const {
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
            return PackSlice<U>(_extent, _begin, 0);
        }

		return PackSlice<U>(_extent, _begin + begin, end - begin);
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

class RefsSlice;

extern const RefsSlice empty_slice;

class BaseRefsSlice : public Slice {
protected:
    const BaseExpressionRef * const _begin;
    const TypeMask _type_mask;

public:
    template<typename V>
    class primitive {
    private:
        const BaseExpressionRef &_u;
    public:
        inline primitive(const BaseExpressionRef &u) : _u(u) {
        }

        inline operator V() const {
            return to_primitive<V>(_u);
        }
    };

    template<typename V>
    using PrimitiveIterator = PointerIterator<BaseExpressionRef, primitive<V>>;

    template<typename V>
    using PrimitiveCollection = PointerCollection<BaseExpressionRef, PrimitiveIterator<V>>;

public:
    using LeafCollection = PointerCollection<BaseExpressionRef, BaseExpressionRef*>;

public:
    template<typename V>
    inline PrimitiveCollection<V> primitives() const {
        return PrimitiveCollection<V>(_begin, _size);
    }

    inline LeafCollection leaves() const {
        return LeafCollection(_begin, _size);
    }

    inline TypeMask type_mask() const {
        return _type_mask;
    }

public:
    inline BaseRefsSlice(const BaseExpressionRef *begin, size_t size, TypeMask type_mask) :
        _begin(begin), Slice(size), _type_mask(type_mask) {
    }

    inline const BaseExpressionRef *begin() const {
        return _begin;
    }

    inline const BaseExpressionRef *end() const {
        return _begin + _size;
    }

    inline const BaseExpressionRef &operator[](size_t i) const {
        return _begin[i];
    }
};

class RefsSlice : public BaseRefsSlice {
private:
	typename RefsExtent::Ref _extent;

    inline RefsSlice(const typename RefsExtent::Ref &extent, TypeMask type_mask) :
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

    inline RefsSlice(const std::vector<BaseExpressionRef> &data, TypeMask type_mask) :
        RefsSlice(std::make_shared<RefsExtent>(data), type_mask) {
	}

	inline RefsSlice(std::vector<BaseExpressionRef> &&data, TypeMask type_mask) :
        RefsSlice(std::make_shared<RefsExtent>(data), type_mask) {
	}

	inline RefsSlice(const std::initializer_list<BaseExpressionRef> &data, TypeMask type_mask) :
        RefsSlice(std::make_shared<RefsExtent>(data), type_mask) {
	}

	inline RefsSlice(
        const typename RefsExtent::Ref &extent,
		const BaseExpressionRef * const begin,
        const BaseExpressionRef * const end,
        TypeMask type_mask) :
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

		if (!_extent) {
			// special case: only 1 item in _expr.
			if (begin > 0 || end < 1) {
				return empty_slice;
			} else {
				return *this;
			}
		}

		const index_t offset = _begin - _extent->address();
		const index_t n = _extent->size();
		assert(offset <= n);

		end = std::min(end, n - offset);
		begin = std::min(begin, end);

        if (end <= begin) {
            return empty_slice;
        }

		return RefsSlice(_extent, _begin + begin, _begin + end, _type_mask);
	}
};

template<size_t N>
class InPlaceRefsSlice : public BaseRefsSlice {
private:
    BaseExpressionRef _refs[N];

public:
    inline InPlaceRefsSlice(const InPlaceRefsSlice<N> &slice) :
        BaseRefsSlice(&_refs[0], slice.size(), slice.type_mask()) {
        // mighty important to provide a copy iterator so that _begin won't get copied.
        std::copy(slice._refs, slice._refs + slice.size(), _refs);
    }

    inline InPlaceRefsSlice(const std::vector<BaseExpressionRef> &refs, TypeMask type_mask) :
        BaseRefsSlice(&_refs[0], N, type_mask) {
        assert(refs.size() == N);
        std::copy(refs.begin(), refs.end(), _refs);
    }

    inline InPlaceRefsSlice(const BaseExpressionRef *refs, size_t size, TypeMask type_mask) :
        BaseRefsSlice(&_refs[0], size, type_mask) {
        assert(size <= N);
        std::copy(refs, refs + size, _refs);
    }

    inline InPlaceRefsSlice<N> slice(index_t begin, index_t end = INDEX_MAX) const {
        const size_t size = _size;

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
        } else {
            return InPlaceRefsSlice<N>(&_refs[begin], end - begin, _type_mask);
        }
    }
};

#endif //CMATHICS_LEAVES_H
