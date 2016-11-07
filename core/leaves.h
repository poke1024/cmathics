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

#include "primitives.h"

class Match;

class Operations {
public:
};

template<typename T>
class OperationsImplementation : public Operations {
private:
	const T &_container;

	template<typename V>
	inline auto primitives() const {
		return _container.template primitives<V>();
	}

	inline auto leaves() const {
		return _container.leaves();
	}

public:
	inline OperationsImplementation(const T &container) : _container(container) {
	}
};

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
		return F(*_ptr);
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

	inline const U *address(size_t index) {
		return &_data[index];
	}
};

template<typename U>
class PackSlice : public Slice, public OperationsImplementation<PackSlice<U>> {
private:
	typename PackExtent<U>::Ref _extent;
	const size_t _offset;

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
	};

	using LeafIterator = PointerIterator<U, leaf>;

	using LeafCollection = PointerCollection<U, LeafIterator>;

public:
	inline PackSlice(const typename PackExtent<U>::Ref &extent, size_t offset, size_t size) :
		_extent(extent), _offset(offset), Slice(size), OperationsImplementation<PackSlice<U>>(*this) {
	}

	template<typename V>
	PrimitiveCollection<V> primitives() const {
		return PrimitiveCollection<V>(_extent->address(_offset), _size);
	}

	LeafCollection leaves() const {
		return LeafCollection(_extent->address(_offset), _size);
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

class RefsSlice : public Slice, public OperationsImplementation<RefsSlice> {
private:
	typename RefsExtent::Ref _extent;
	const BaseExpressionRef _expr;
	const BaseExpressionRef * const _begin;

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
	PrimitiveCollection<V> primitives() const {
		return PrimitiveCollection<V>(_begin, _size);
	}

	LeafCollection leaves() const {
		return LeafCollection(_begin, _size);
	}

public:
	inline RefsSlice() :
		_begin(nullptr), Slice(0), OperationsImplementation<RefsSlice>(*this) {
	}

	inline RefsSlice(const std::vector<BaseExpressionRef> &data) :
		_extent(std::make_shared<RefsExtent>(data)), _begin(_extent->address()), Slice(data.size()),
		OperationsImplementation<RefsSlice>(*this) {
	}

	inline RefsSlice(std::vector<BaseExpressionRef> &&data) :
		_extent(std::make_shared<RefsExtent>(data)), _begin(_extent->address()), Slice(data.size()),
		OperationsImplementation<RefsSlice>(*this) {
	}

	inline RefsSlice(const std::initializer_list<BaseExpressionRef> &data) :
		_extent(std::make_shared<RefsExtent>(data)), _begin(_extent->address()), Slice(data.size()),
		OperationsImplementation<RefsSlice>(*this) {
	}

	inline RefsSlice(const typename RefsExtent::Ref &extent,
		const BaseExpressionRef * const begin, const BaseExpressionRef * const end) :
		_extent(extent), _begin(begin), Slice(end - begin),
		OperationsImplementation<RefsSlice>(*this) {
	}

	inline RefsSlice(const RefsSlice &slice) :
		_extent(slice._extent), _expr(slice._expr),
		_begin(_expr ? &_expr : slice._begin), Slice(slice._size),
		OperationsImplementation<RefsSlice>(*this) {
	}

	inline RefsSlice(const BaseExpressionRef &expr) :
		_expr(expr), _begin(&_expr), Slice(1),
		OperationsImplementation<RefsSlice>(*this) {
		// special case for Slices with exactly one element. happens extensively during
		// evaluation when we pattern match rules, so it's very useful to optimize this.
	}

	inline operator bool() const {
		return _begin;
	}

	inline bool operator==(const RefsSlice &slice) const {
		if (_expr) {
			return _expr == slice._expr;
		} else {
			return _begin == slice._begin && _size == slice._size;
		}
	}

	inline bool operator!=(const RefsSlice &slice) const {
		return !(*this == slice);
	}

	RefsSlice slice(size_t begin, size_t end = SIZE_T_MAX) const;

	template<typename F>
	RefsSlice apply(
			size_t begin,
			size_t end,
			const F &f,
			TypeMask mask) const {

		assert(_expr == nullptr);

		if (_extent.use_count() == 1) {
			// FIXME. optimize this case. we do not need to copy here.
		}

		auto leaves = _extent->address();
		for (size_t i = begin; i < end; i++) {
			// FIXME check mask

			auto new_leaf = f(leaves[i]);

			if (new_leaf) { // copy is needed now
				std::vector<BaseExpressionRef> new_leaves;
				new_leaves.reserve(_size);

				new_leaves.insert(new_leaves.end(), &leaves[0], &leaves[i]);

				new_leaves.push_back(new_leaf); // leaves[i]

				for (size_t j = i + 1; j < end; j++) {
					// FIXME check mask

					auto old_leaf = leaves[j];
					new_leaf = f(old_leaf);
					if (new_leaf) {
						new_leaves.push_back(new_leaf);
					} else {
						new_leaves.push_back(old_leaf);
					}
				}

				new_leaves.insert(new_leaves.end(), &leaves[end], &leaves[_size]);

				return RefsSlice(std::move(new_leaves));
			}
		}

		return *this;
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

extern const RefsSlice empty_slice;

#endif //CMATHICS_LEAVES_H
