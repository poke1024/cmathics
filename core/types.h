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

typedef int64_t match_size_t; // needs to be signed
typedef std::tuple<match_size_t, match_size_t> match_sizes_t;
constexpr match_size_t MATCH_MAX = INT64_MAX;

class Definitions;
class Symbol;

typedef std::vector<std::tuple<const Symbol*, BaseExpressionRef>> Variables;

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

    void add_variable(const Symbol *var, const BaseExpressionRef &value) {
        if (!_variables) {
            _variables = std::make_shared<Variables>();
        }
        _variables->push_back(std::make_tuple(var, value));
    }
};

std::ostream &operator<<(std::ostream &s, const Match &m);

class Matcher;

class Expression;

typedef std::shared_ptr<const Expression> ExpressionRef;

class BaseExpression : public std::enable_shared_from_this<BaseExpression> {
public:
    BaseExpression() {
    }

    virtual ~BaseExpression() {
    }

    virtual Type type() const = 0;
    // virtual bool is_symbol() const;
    // virtual bool is_expression() const;
    virtual bool is_symbol_sequence() const {
        return false;
    }

    inline bool same(const BaseExpressionRef &expr) const {
        return same(expr.get());
    }

    virtual bool same(BaseExpressionPtr expr) const = 0;

    virtual hash_t hash() const = 0;

    virtual std::string fullform() const = 0;

    virtual BaseExpressionRef evaluate() const {
        // atomic expressions remain unchanged
        return nullptr;
    }

    // various getters

    virtual BaseExpressionRef get_head() const {
        return BaseExpressionRef(); // no head available
    }

    virtual BaseExpressionPtr get_head_ptr() const {
        return nullptr; // no head available
    }

    virtual bool is_sequence() const {
        return false;
    }

    virtual const char *get_string_value() const {
        return nullptr;
    }

    // pattern matching; if not noted otherwise, "this" is the pattern that is matched against here.

    virtual Match match_sequence(const Matcher &matcher) const;

    virtual Match match_sequence_with_head(const ExpressionRef &patt, const Matcher &matcher) const;

    virtual match_sizes_t match_num_args() const {
        return std::make_tuple(1, 1); // default
    }

    virtual match_sizes_t match_num_args_with_head(const Expression *patt) const {
        return std::make_tuple(1, 1); // default for non-symbol heads
    }

    virtual BaseExpressionRef replace(BaseExpressionRef item) const {
        // replace expression at base
        // Equivalent to Replace[expr, patt] but doesn't search into the expression
        // e.g. DoReplace(1 + a, a -> 2) doesn't apply because it doesn't match at root
        // returns NULL if no match is found
        // TODO
        return nullptr;
    }
};

inline const BaseExpressionRef *copy_leaves(const BaseExpressionRef *leaves, size_t n) {
    auto new_leaves = new BaseExpressionRef[n];
    for (size_t i = 0; i < n; i++) {
        new_leaves[i] = leaves[i];
    }
    return new_leaves;
}

#endif
