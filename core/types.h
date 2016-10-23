#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <stdint.h>
#include <functional>
#include <vector>

#include "hash.h"
#include "leaves.h"

enum Type {
    MachineIntegerType,
    BigIntegerType,
    MachineRealType,
    BigRealType,
    RationalType,
    ComplexType,
    ExpressionType,
    SymbolType,
    StringType,
    RawExpressionType
};

typedef std::tuple<size_t, size_t> match_sizes_t;
constexpr size_t MATCH_MAX = SIZE_T_MAX;

class Symbol;

typedef std::vector<std::tuple<const Symbol*, BaseExpressionPtr>> Variables;

class Match {
private:
    bool _matched;
    std::shared_ptr<Variables> _variables;

public:
    inline Match() : _matched(false) {
    }

    inline Match(bool matched) : _matched(matched) {
    }

    inline operator bool() const {
        return _matched;
    }

    inline const Variables *vars() const {
        return _variables.get();
    }

    void add_variable(const Symbol *var, BaseExpressionPtr value) {
        if (!_variables) {
            _variables = std::make_shared<Variables>();
        }
        _variables->push_back(std::make_tuple(var, value));
    }
};

std::ostream &operator<<(std::ostream &s, const Match &m);

class BaseExpression {
private:
    mutable size_t _ref; // what we really want here is boost::intrusive_ptr

public:
    BaseExpression() : _ref(0) {
    }

    virtual ~BaseExpression() {
    }

    inline BaseExpressionPtr ref() const {
        _ref++;
        return this;
    }

    inline void deref() const {
        if (--_ref == 0) {
            delete this;
        }
    }

    virtual Type type() const = 0;
    // virtual bool is_symbol() const;
    // virtual bool is_expression() const;
    virtual bool is_symbol_sequence() const {
        return false;
    }

    virtual bool same(const BaseExpression *expr) const = 0;

    virtual hash_t hash() const = 0;

    virtual std::string fullform() const = 0;

    virtual BaseExpressionPtr evaluate() const {
        // atomic expressions remain unchanged
        return nullptr;
    }

    // various getters

    virtual Slice get_sequence() const {
        return Slice(this);
    }

    virtual const char *get_string_value() const {
        return nullptr;
    }

    // pattern matching; if not noted otherwise, "this" is the pattern that is matched against here.

    virtual Match match(const BaseExpression *item) const {
        return Match(false); // match "item" against this
    }

    virtual Match match_sequence(
        const Slice &pattern,
        const Slice &sequence) const;

    virtual Match match_sequence_with_head(
        const Slice &pattern,
        const Slice &sequence) const;

    Match match_rest(
        const Slice &pattern,
        const Slice &sequence,
        size_t match_size = 1) const;

    virtual match_sizes_t match_num_args() const {
        return std::make_tuple(1, 1); // default
    }

    virtual match_sizes_t match_num_args_with_head(const Expression *patt) const {
        return std::make_tuple(1, 1); // default for non-symbol heads
    }

    virtual BaseExpression *replace(const BaseExpression *item) const {
        // replace expression at base
        // Equivalent to Replace[expr, patt] but doesn't search into the expression
        // e.g. DoReplace(1 + a, a -> 2) doesn't apply because it doesn't match at root
        // returns NULL if no match is found
        // TODO
        return nullptr;
    }
};

inline const BaseExpressionPtr *copy_leaves(const BaseExpressionPtr *leaves, size_t n) {
    auto new_leaves = new BaseExpressionPtr[n];
    for (size_t i = 0; i < n; i++) {
        new_leaves[i] = leaves[i]->ref();
    }
    return new_leaves;
}

#endif
