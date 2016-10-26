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

class BaseExpression;
class Expression;
class Match;

typedef const BaseExpression* BaseExpressionPtr;
typedef std::shared_ptr<const BaseExpression> BaseExpressionRef;

const BaseExpressionRef *copy_leaves(const BaseExpressionRef *leaves, size_t n);

class Extent {
protected:
    friend class Slice;

    const BaseExpressionRef *_leaves;
    const size_t _n;

public:
    inline Extent(const BaseExpressionRef *leaves, size_t n) : _n(n), _leaves(copy_leaves(leaves, n)) {
    }

    ~Extent();

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
    BaseExpressionRef _expr;
    const BaseExpressionRef * const _begin;
    const BaseExpressionRef * const _end;

public:
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

    inline Slice(const Slice& leaves) :
        _storage(leaves._storage),
        _begin(leaves._begin),
        _end(leaves._end) {
    }

    inline Slice(const BaseExpressionRef &expr) :
        _expr(expr),
        _begin(&expr),
        _end(_begin + 1) {
        // special case for Leaves with exactly one element. happens often enough to optimize this.
    }

    inline const BaseExpressionRef &leaf(size_t i) const {
        return _begin[i];
    }

    inline BaseExpressionPtr leaf_ptr(size_t i) const {
        return _begin[i].get();
    }

    inline bool operator==(const Slice &leaves) const {
        return _begin == leaves._begin && _end == leaves._end && _expr == leaves._expr;
    }

    Slice apply(
        size_t begin,
        size_t end,
        const std::function<BaseExpressionRef(const BaseExpressionRef&)> &f) const;

    Slice slice(size_t begin, size_t end = SIZE_T_MAX) const {
        assert(begin <= end);

        const size_t offset = _begin - _storage->_leaves + begin;
        const size_t n = _storage->_n;
        assert(offset <= n);

        end = std::min(end, n - offset);
        return Slice(_storage, _begin + begin, _begin + end);
    }

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
