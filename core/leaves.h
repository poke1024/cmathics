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
const BaseExpressionPtr *copy_leaves(const BaseExpressionPtr *leaves, size_t n);

class Extent {
public: // protected:
    const BaseExpressionPtr *_leaves;
    const size_t _n;

public:
    inline Extent(const BaseExpressionPtr *leaves, size_t n) : _n(n), _leaves(copy_leaves(leaves, n)) {
    }

    ~Extent();
};

class Slice {
public: // private:
    const std::shared_ptr<Extent> _storage;
    const BaseExpressionPtr * const _begin;
    const BaseExpressionPtr * const _end;
    const BaseExpressionPtr _expr;

public:
    // copy leaves. Leaves takes ownership of the BaseExpression instances in leaves and will delete them.
    inline Slice(const BaseExpressionPtr* leaves, size_t n) :
        _storage(std::make_shared<Extent>(leaves, n)),
        _begin(_storage->_leaves),
        _end(_begin + n),
        _expr(nullptr) {
    }

    // refer to the leaves of an already existing storage.
    inline Slice(const std::shared_ptr<Extent> &storage) :
        _storage(storage),
        _begin(_storage->_leaves),
        _end(_begin + _storage->_n),
        _expr(nullptr) {
    }

    // refer to a slice of an already existing storage.
    inline Slice(
        const std::shared_ptr<Extent> &storage,
        const BaseExpressionPtr * const begin,
        const BaseExpressionPtr * const end) :

        _storage(storage),
        _begin(begin),
        _end(end),
        _expr(nullptr) {
    }

    inline Slice(const Slice& leaves) :
        _storage(leaves._storage),
        _begin(leaves._begin),
        _end(leaves._end),
        _expr(nullptr) {
    }

    inline Slice(BaseExpressionPtr expr) :
        _expr(expr),
        _begin(const_cast<BaseExpressionPtr*>(&_expr)),
        _end(_begin + 1) {
        // special case for Leaves with exactly one element. happens often enough to optimize this.
    }

    inline bool operator==(const Slice &leaves) const {
        return _begin == leaves._begin && _end == leaves._end && _expr == leaves._expr;
    }

    Slice apply(
        size_t begin,
        size_t end,
        const std::function<BaseExpressionPtr(BaseExpressionPtr)> &f) const;

    void subranges(
        size_t min_count,
        size_t max_count = SIZE_T_MAX,
        bool flexible_start = false,
        bool less_first = false) const;

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

    inline const BaseExpressionPtr *begin() const {
        return _begin;
    }

    inline const BaseExpressionPtr *end() const {
        return _end;
    }

    inline BaseExpressionPtr operator[](size_t i) const {
        return _begin[i];
    }
};

extern const Slice empty_slice;

#endif //CMATHICS_LEAVES_H
