#ifndef CMATHICS_SYMBOL_H_H
#define CMATHICS_SYMBOL_H_H

#include "types.h"
#include "rule.h"

#include <string>
#include <vector>
#include <map>
#include <bitset>

#include <symengine/symbol.h>
#include <symengine/constants.h>

typedef uint32_t attributes_bitmask_t;

enum class Attributes : attributes_bitmask_t {
	None = 0,
	// pattern matching attributes
	Orderless = 1 << 0,
	Flat = 1 << 1,
	OneIdentity = 1 << 2,
	Listable = 1 << 3,
	// calculus attributes
	Constant = 1 << 4,
	NumericFunction = 1 << 5,
	// rw attributes
	Protected = 1 << 6,
	Locked = 1 << 7,
	ReadProtected = 1 << 8,
	// evaluation hold attributes
	HoldFirst = 1 << 9,
	HoldRest = 1 << 10,
	HoldAll = HoldFirst + HoldRest,
	HoldAllComplete = 1 << 11,
	// evaluation nhold attributes
	NHoldFirst = 1 << 12,
	NHoldRest = 1 << 13,
	NHoldAll = NHoldFirst + NHoldRest,
	// misc attributes
	SequenceHold = 1 << 14,
	Temporary = 1 << 15,
	Stub = 1 << 16
};

inline bool
operator&(Attributes x, Attributes y)  {
	return (static_cast<attributes_bitmask_t>(x) & static_cast<attributes_bitmask_t>(y)) != 0;
}

inline size_t
count(Attributes x, Attributes y) {
	std::bitset<sizeof(attributes_bitmask_t) * 8> bits(
		static_cast<attributes_bitmask_t>(x) & static_cast<attributes_bitmask_t>(y));
	return bits.count();
}

inline constexpr Attributes
operator+(Attributes x, Attributes y)  {
	return static_cast<Attributes>(
		static_cast<attributes_bitmask_t>(x) | static_cast<attributes_bitmask_t>(y));
}

class Definitions;

class Evaluate;

class Messages {
private:
	Rules m_rules;

public:
	void add(const SymbolRef &name, const char *tag, const char *text, const Definitions &definitions);

	StringRef lookup(const Expression *message, const Evaluation &evaluation) const;
};

typedef std::shared_ptr<Messages> MessagesRef;

struct SymbolRules {
	Rules sub_rules;
	Rules up_rules;
	Rules down_rules;
	MessagesRef messages;

	/*
	Expression* n_values;
	Expression* format_values;
	Expression* default_values;
	Expression* options;
	*/
};

typedef std::unique_ptr<SymbolRules> SymbolRulesRef;

class Symbol : public BaseExpression {
protected:
	friend class Definitions;

	char _short_name[32];
	char *_name;

	mutable MatchId _match_id;
	mutable BaseExpressionRef _match_value;
	mutable Symbol *_linked_variable;

	mutable const BaseExpressionRef *_replacement;

	Attributes _attributes;
	const Evaluate *_evaluate_with_head;

	SymbolRulesRef _rules;

	inline SymbolRules *create_rules() {
		if (!_rules) {
			_rules = std::make_unique<SymbolRules>();
		}
		return _rules.get();
	}

protected:
    virtual bool instantiate_symbolic_form() const;

public:
	Symbol(const char *name, ExtendedType symbol = SymbolExtendedType);

	~Symbol();

	virtual BaseExpressionPtr head(const Evaluation &evaluation) const final;

	inline const Evaluate &evaluate_with_head() const {
		return *_evaluate_with_head;
	}

	BaseExpressionRef own_value;

	virtual inline bool same(const BaseExpression &expr) const final {
		// compare as pointers: Symbol instances are unique
		return &expr == this;
	}

	virtual hash_t hash() const {
		return hash_pair(symbol_hash, (std::uintptr_t)this);
	}

	inline SymbolRules *rules() const {
		return _rules.get();
	}

	virtual std::string fullform() const {
		return name();
	}

	inline const char *short_name() const {
		const char *s = name();
		const char *short_string = s;
		while (*s) {
			if (*s == '`') {
				short_string = s + 1;
			}
			s++;
		}
		return short_string;
	};

	inline const char *name() const {
		return _name;
	}

	inline BaseExpressionRef evaluate_symbol() const {
		return own_value;
	}

	inline void add_down_rule(const RuleRef &rule) {
		create_rules()->down_rules.add(rule);
	}

	inline void add_sub_rule(const RuleRef &rule) {
		create_rules()->sub_rules.add(rule);
	}

	void add_message(
		const char *tag,
		const char *text,
		const Definitions &definitions);

	StringRef lookup_message(
		const Expression *message,
		const Evaluation &evaluation) const;

	virtual bool match(const BaseExpression &expr) const {
		return same(expr);
	}

	virtual BaseExpressionRef replace_all(const Match &match) const;

	inline bool set_matched_value(const MatchId &id, const BaseExpressionRef &value) const {
		if (_match_id == id) {
			return _match_value->same(value.get());
		} else {
			_match_id = id;
			_match_value = value;
			return true;
		}
	}

	inline void clear_matched_value() const {
		_match_id.reset();
		_match_value.reset();
	}

	inline BaseExpressionRef matched_value() const {
		// only call this after a successful match and only on variables that were actually
		// matched. during matching, i.e. if the match is not yet successful, the MatchId
		// needs to be checked to filter out old, stale matches using the getter function
		// just below this one.
		return _match_value;
	}

	/*inline BaseExpressionRef matched_value(const MatchId &id) const {
		if (_match_id == id) {
			return _match_value;
		} else {
			return BaseExpressionRef();
		}
	}*/

	inline void set_next_variable(Symbol *symbol) const {
		_linked_variable = symbol;
	}

	inline Symbol *next_variable() const {
		return _linked_variable;
	}

	inline void set_replacement(const BaseExpressionRef *r) const {
		_replacement = r;
	}

	inline void clear_replacement() const {
		_replacement = nullptr;
	}

	inline const BaseExpressionRef *replacement() const {
		return _replacement;
	}

	void set_attributes(Attributes a);

	virtual const Symbol *lookup_name() const {
		return this;
	}

	void add_rule(BaseExpressionPtr lhs, BaseExpressionPtr rhs);

	void add_rule(const RuleRef &rule);
};

class SymbolKey {
private:
	const SymbolRef _symbol;
	const char * const _name;

public:
	inline SymbolKey(SymbolRef symbol) : _symbol(symbol), _name(nullptr) {
	}

	inline SymbolKey(const char *name) : _name(name) {
	}

	inline bool operator==(const SymbolKey &key) const {
		if (_symbol) {
			if (key._symbol) {
				return _symbol == key._symbol;
			} else {
				return strcmp(_symbol->name(), key._name) == 0;
			}
		} else {
			if (key._symbol) {
				return strcmp(_name, key._symbol->name()) == 0;
			} else {
				return strcmp(_name, key._name) == 0;
			}
		}
	}

	inline const char *c_str() const {
		return _name ? _name : _symbol->name();
	}
};

namespace std {
	// for the following hash, also see http://stackoverflow.com/questions/98153/
	// whats-the-best-hashing-algorithm-to-use-on-a-stl-string-when-using-hash-map

	template<>
	struct hash<SymbolKey> {
		inline size_t operator()(const SymbolKey &key) const {
			const char *s = key.c_str();

			size_t hash = 0;
			while (*s) {
				hash = hash * 101 + *s++;
			}

			return hash;
		}
	};
}

inline BaseExpressionRef BaseExpression::evaluate(
	const Evaluation &evaluation) const {

    BaseExpressionRef result;

    while (true) {
        const BaseExpression *expr = result ? result.get() : this;

        switch (expr->type()) {
            case ExpressionType: {
                BaseExpressionRef form =
                    static_cast<const Expression*>(expr)->evaluate_expression(evaluation);
                if (form) {
                    result = std::move(form);
                } else {
                    return result;
                }
                break;
            }

            case SymbolType: {
                BaseExpressionRef form =
                    static_cast<const Symbol*>(expr)->evaluate_symbol();
                if (form) {
                    result = std::move(form);
                } else {
                    return result;
                }
            }

            default:
                return result;
        }
    }
}

inline BaseExpressionRef BaseExpression::evaluate_or_copy(
    const Evaluation &evaluation) const {

    const BaseExpressionRef result = evaluate(evaluation);
    if (result) {
        return result;
    } else {
        return BaseExpressionRef(this);
    }
}

inline Symbol *BaseExpression::as_symbol() const {
    return const_cast<Symbol*>(static_cast<const Symbol*>(this));
}

template<typename F>
inline BaseExpressionRef scope(
	Symbol *symbol,
	BaseExpressionRef &&value,
	const F &f) {

    BaseExpressionRef swapped(value);
    std::swap(swapped, symbol->own_value);

	try {
		const BaseExpressionRef result = f();
        std::swap(swapped, symbol->own_value);
        return result;
	} catch(...) {
        std::swap(swapped, symbol->own_value);
		throw;
	}
}

template<typename F>
inline auto scoped(
	Symbol *symbol,
	const F &f) {

	return [symbol, &f] (BaseExpressionRef &&value) {
		return scope(symbol, std::move(value), f);
	};
}

#endif //CMATHICS_SYMBOL_H_H
