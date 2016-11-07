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

class BaseExpression;
class Expression;
class Match;

typedef const BaseExpression* BaseExpressionPtr;
typedef std::shared_ptr<const BaseExpression> BaseExpressionRef;

typedef uint16_t TypeMask;

inline const BaseExpressionRef *copy_leaves(const BaseExpressionRef *leaves, size_t n) {
    auto new_leaves = new BaseExpressionRef[n];
    for (size_t i = 0; i < n; i++) {
        new_leaves[i] = leaves[i];
    }
    return new_leaves;
}

class Extent {
protected:
    friend class Slice;

    const BaseExpressionRef *_leaves;
    const size_t _n;

public:
    inline Extent(const BaseExpressionRef *leaves, size_t n) : _n(n), _leaves(copy_leaves(leaves, n)) {
    }

    inline ~Extent() {
	    delete[] _leaves;
    }

    inline const BaseExpressionRef &leaf(size_t i) const {
        return _leaves[i];
    }

    inline BaseExpressionPtr leaf_ptr(size_t i) const {
        return _leaves[i].get();
    }
};

class Slice {
public: // private:
    const std::shared_ptr<Extent> _storage;
    const BaseExpressionRef _expr;
    const BaseExpressionRef * const _begin;
    const BaseExpressionRef * const _end;

public:
    inline explicit Slice() : _begin(nullptr), _end(nullptr) {
    }

    // copy leaves. Leaves takes ownership of the BaseExpression instances in leaves and will delete them.
    inline Slice(const BaseExpressionRef* leaves, size_t n) :
        _storage(std::make_shared<Extent>(leaves, n)),
        _begin(_storage->_leaves),
        _end(_begin + n) {
    }

    // refer to the leaves of an already existing storage.
    inline Slice(const std::shared_ptr<Extent> &storage) :
        _storage(storage),
        _begin(_storage->_leaves),
        _end(_begin + _storage->_n) {
    }

    // refer to a slice of an already existing storage.
    inline Slice(
        const std::shared_ptr<Extent> &storage,
        const BaseExpressionRef * const begin,
        const BaseExpressionRef * const end) :

        _storage(storage),
        _begin(begin),
        _end(end) {
    }

    inline Slice(const Slice& slice) :
        _storage(slice._storage),
        _expr(slice._expr),
        _begin(_expr ? &_expr : slice._begin),
        _end(_expr ? _begin + 1 : slice._end) {
    }

    inline Slice(const BaseExpressionRef &expr) :
        _expr(expr),
        _begin(&_expr),
        _end(_begin + 1) {
        // special case for Leaves with exactly one element. happens extensively during
        // evaluation when we pattern match rules, so it's very useful to optimize this.
    }

    inline operator bool() const {
        return _begin;
    }

    inline const BaseExpressionRef &leaf(size_t i) const {
        return _begin[i];
    }

    inline BaseExpressionPtr leaf_ptr(size_t i) const {
        return _begin[i].get();
    }

    inline bool operator==(const Slice &slice) const {
        if (_expr) {
            return _expr == slice._expr;
        } else {
            return _begin == slice._begin && _end == slice._end;
        }
    }

	inline bool operator!=(const Slice &slice) const {
		return !(*this == slice);
	}

	template<typename F>
    Slice apply(
        size_t begin,
        size_t end,
        const F &f,
        TypeMask mask) const {

	    assert(_expr == nullptr);

	    if (_storage.use_count() == 1) {
		    // FIXME. optimize this case. we do not need to copy here.
	    }

	    auto leaves = _storage->_leaves;
	    for (size_t i = begin; i < end; i++) {
		    // FIXME check mask

		    auto new_leaf = f(leaves[i]);

		    if (new_leaf) { // copy is needed now
			    std::vector<BaseExpressionRef> new_leaves;
			    new_leaves.reserve(_end - _begin);

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

			    new_leaves.insert(new_leaves.end(), &leaves[end], &leaves[_end - _begin]);

			    return Slice(std::make_shared<Extent>(&new_leaves[0], new_leaves.size()));
		    }
	    }

	    return *this;
    }

    Slice slice(size_t begin, size_t end = SIZE_T_MAX) const;

    inline size_t size() const {
        return _end - _begin;
    }

    inline const BaseExpressionRef *begin() const {
        return _begin;
    }

    inline const BaseExpressionRef *end() const {
        return _end;
    }

    inline BaseExpressionRef operator[](size_t i) const {
        return _begin[i];
    }
};

extern const Slice empty_slice;

#endif //CMATHICS_LEAVES_H
