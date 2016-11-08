#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <stdint.h>
#include <functional>
#include <vector>
#include <cstdlib>
#include "hash.h"

class BaseExpression;
typedef const BaseExpression* BaseExpressionPtr;
typedef std::shared_ptr<const BaseExpression> BaseExpressionRef;

enum Type {
    MachineIntegerType,
    BigIntegerType,
    MachineRealType,
    BigRealType,
    RationalType,
    ComplexType,
    ExpressionType,
    SymbolType,
    StringType
};

typedef uint16_t TypeMask;

typedef int64_t index_t; // may be negative as well
constexpr index_t INDEX_MAX = INT64_MAX;

const char *type_name(Type type);

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

class MatchId { // uniquely identifies a specific match
private:
	std::weak_ptr<const BaseExpression> _patt;
	std::weak_ptr<const BaseExpression> _item;

public:
	inline MatchId() {
	}

	inline MatchId(const MatchId& id) : _patt(id._patt), _item(id._item) {
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

class MatchContext {
private:
	const Symbol *_matched_variables_head;

public:
	const MatchId id;
	Definitions &definitions;

	inline MatchContext(const BaseExpressionRef &patt, const BaseExpressionRef &item, Definitions &defs) :
		id(patt, item), definitions(defs), _matched_variables_head(nullptr) {
	}

	void add_matched_variable(const Symbol *variable);

	inline const Symbol *matched_variables() const {
		return _matched_variables_head;
	}
};

class Match {
private:
    const bool _matched;
	const MatchId _id;
    const Symbol *_variables;

public:
	explicit inline Match() : _matched(false) {
	}

    explicit inline Match(bool matched, const MatchContext &context) :
	    _matched(matched), _id(context.id), _variables(context.matched_variables()) {
    }

    inline operator bool() const {
        return _matched;
    }

	inline const MatchId &id() const {
		return _id;
	}

    inline const Symbol *variables() const {
        return _variables;
    }

    template<int N>
    typename BaseExpressionTuple<N>::type get() const;
};

std::ostream &operator<<(std::ostream &s, const Match &m);

class Matcher;

class Expression;
typedef std::shared_ptr<const Expression> ExpressionRef;
typedef const Expression *ExpressionPtr;

template<typename S>
class ExpressionImplementation;
class RefsSlice;
typedef const ExpressionImplementation<RefsSlice> RefsExpression;
typedef const RefsExpression* RefsExpressionPtr;
typedef std::shared_ptr<const RefsExpression> RefsExpressionRef;

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

	inline TypeMask type_mask() const {
		return ((TypeMask)1) << _type;
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

    virtual BaseExpressionRef evaluate(const BaseExpressionRef &self, const Evaluation &evaluation) const {
        return BaseExpressionRef(); // atomic expressions remain unchanged
    }

    virtual BaseExpressionRef evaluate_once(const BaseExpressionRef &self, const Evaluation &evaluation) const {
        return BaseExpressionRef(); // atomic expressions remain unchanged
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

    virtual bool match_sequence_with_head(RefsExpressionPtr patt, const Matcher &matcher) const;

    virtual match_sizes_t match_num_args() const {
        return std::make_tuple(1, 1); // default
    }

    virtual match_sizes_t match_num_args_with_head(ExpressionPtr patt) const {
        return std::make_tuple(1, 1); // default for non-symbol heads
    }

    virtual BaseExpressionRef replace_all(const Match &match) const {
		return BaseExpressionRef();
    }

    virtual BaseExpressionRef clone() const {
        throw std::runtime_error("not implemented yet");
    }

	virtual BaseExpressionRef clone(const BaseExpressionRef &head) const {
		throw std::runtime_error("cannot clone with head");
	}

	virtual RefsExpressionRef to_refs_expression(BaseExpressionRef self) const {
		throw std::runtime_error("cannot create refs expression");
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

class ExpressionIterator;

class OperationsInterface;

class Expression : public BaseExpression {
public:
	const BaseExpressionRef _head;

	inline Expression(const BaseExpressionRef &head) : BaseExpression(ExpressionType), _head(head) {
	}

	// virtual const Operations &operations() const = 0;

	virtual size_t size() const = 0;

	virtual BaseExpressionRef leaf(size_t i) const = 0;

	inline ExpressionIterator begin() const;

	inline ExpressionIterator end() const;

	virtual BaseExpressionRef head() const {
		return _head;
	}

	virtual BaseExpressionPtr head_ptr() const {
		return _head.get();
	}

	virtual bool is_sequence() const {
		return _head->is_symbol_sequence();
	}

	virtual BaseExpressionRef evaluate_values(const ExpressionRef &self, const Evaluation &evaluation) const = 0;

	virtual ExpressionRef slice(index_t begin, index_t end = INDEX_MAX) const = 0;

    virtual const OperationsInterface &operations() const = 0;
};

class ExpressionIterator {
private:
	const Expression * const _expr;
	size_t _pos;

public:
	inline ExpressionIterator() : _expr(nullptr) {
	}

	inline explicit ExpressionIterator(const Expression *expr, size_t pos) : _expr(expr), _pos(pos) {
	}

	inline auto operator*() const {
		return _expr->leaf(_pos);
	}

	inline bool operator==(const ExpressionIterator &other) const {
		return _expr == other._expr && _pos == other._pos;
	}

	inline bool operator!=(const ExpressionIterator &other) const {
		return _expr != other._expr || _pos != other._pos;
	}

	inline ExpressionIterator &operator++() {
		_pos += 1;
		return *this;
	}

	inline ExpressionIterator operator++(int) const {
		return ExpressionIterator(_expr, _pos + 1);
	}
};

inline ExpressionIterator Expression::begin() const {
	return ExpressionIterator(this, 0);
}

inline ExpressionIterator Expression::end() const {
	return ExpressionIterator(this, size());
}

#endif
