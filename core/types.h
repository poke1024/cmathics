#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <stdint.h>
#include <functional>
#include <vector>
#include <cstdlib>

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

typedef int64_t machine_integer_t;
typedef double machine_real_t;

typedef int64_t match_size_t; // needs to be signed
typedef std::tuple<match_size_t, match_size_t> match_sizes_t;
constexpr match_size_t MATCH_MAX = INT64_MAX;

class Definitions;
class Symbol;

template<int N, typename... refs>
struct BaseExpressionTuple {
    typedef typename BaseExpressionTuple<N - 1, BaseExpressionRef, refs...>::type type;
};

template<typename... refs>
struct BaseExpressionTuple<0, refs...> {
    typedef std::tuple<refs...> type;
};

class Match {
private:
    bool _matched;
    const Symbol *_variables;

public:
    explicit inline Match(bool matched, const Symbol *variables) : _matched(matched), _variables(variables) {
    }

    inline operator bool() const {
        return _matched;
    }

    inline const Symbol *variables() const {
        return _variables;
    }

    template<int N>
    typename BaseExpressionTuple<N>::type get() const;
};

std::ostream &operator<<(std::ostream &s, const Match &m);

class MatchId { // uniquely identifies a specific match
private:
    std::weak_ptr<const BaseExpression> _patt;
    std::weak_ptr<const BaseExpression> _item;

public:
    inline MatchId() {
    }

    inline MatchId(const BaseExpressionRef &patt, const BaseExpressionRef &item) : _patt(patt), _item(item) {
    }

    inline MatchId &operator=(const MatchId &other) {
        _patt = other._patt;
        _item = other._item;
        return *this;
    }

    inline bool operator==(const MatchId &other) const {
        if (_patt.expired() || _item.expired() || other._patt.expired() || other._item.expired()) {
            return false;
        } else {
            return _patt.lock().get() == other._patt.lock().get() &&
                _item.lock().get() == other._item.lock().get();
        }
    }

    inline void reset() {
        _patt.reset();
        _item.reset();
    }
};

class Matcher;

class Expression;

typedef std::shared_ptr<const Expression> ExpressionRef;
typedef const Expression *ExpressionPtr;

class Symbol;

// in contrast to BaseExpressionRef, SymbolRefs are not constant. Symbols might
// change, after all, if rules are added.
typedef std::shared_ptr<Symbol> SymbolRef;

class Evaluation;

class BaseExpression {
private:
	const Type _type;

public:
    inline BaseExpression(Type type) : _type(type) {
    }

    virtual ~BaseExpression() {
    }

    inline Type type() const {
	    return _type;
    }

    // virtual bool is_symbol() const;
    // virtual bool is_expression() const;
    virtual bool is_symbol_sequence() const {
        return false;
    }

    inline bool same(const BaseExpressionRef &expr) const {
        return same(*expr);
    }

    virtual bool same(const BaseExpression &expr) const = 0;

    virtual hash_t hash() const = 0;

    virtual std::string fullform() const = 0;

    virtual BaseExpressionRef evaluate(const BaseExpressionRef &self, Evaluation &evaluation) const {
        // atomic expressions remain unchanged
        return BaseExpressionRef();
    }

    // various getters

    virtual BaseExpressionRef head() const {
        return BaseExpressionRef(); // no head available
    }

    virtual BaseExpressionPtr head_ptr() const {
        return nullptr; // no head available
    }

    virtual bool is_sequence() const {
        return false;
    }

    virtual const char *get_string_value() const {
        return nullptr;
    }

    // pattern matching; if not noted otherwise, "this" is the pattern that is matched against here.

    virtual bool match_sequence(const Matcher &matcher) const;

    virtual bool match_sequence_with_head(ExpressionPtr patt, const Matcher &matcher) const;

    virtual match_sizes_t match_num_args() const {
        return std::make_tuple(1, 1); // default
    }

    virtual match_sizes_t match_num_args_with_head(ExpressionPtr patt) const {
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

    virtual BaseExpressionRef clone() const {
        throw std::runtime_error("not implemented yet");
    }
};

inline std::ostream &operator<<(std::ostream &s, const BaseExpressionRef &expr) {
    if (expr) {
        s << expr->fullform();
    } else {
        //s << "(no expression)";
    }
    return s;
}


inline const BaseExpressionRef *copy_leaves(const BaseExpressionRef *leaves, size_t n) {
    auto new_leaves = new BaseExpressionRef[n];
    for (size_t i = 0; i < n; i++) {
        new_leaves[i] = leaves[i];
    }
    return new_leaves;
}

#endif
