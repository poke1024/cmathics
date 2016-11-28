#ifndef CMATHICS_SYMBOL_H_H
#define CMATHICS_SYMBOL_H_H

#include "types.h"
#include "rule.h"

#include <string>
#include <vector>
#include <map>
#include <bitset>

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

public:
	Symbol(const char *name, ExtendedType symbol = SymbolExtendedType);

	~Symbol();

	/*Expression* own_values;
	Expression* sub_values;
	Expression* up_values;
	Expression* down_values;
	Expression* n_values;
	Expression* format_values;
	Expression* default_values;
	Expression* messages;
	Expression* options;*/

	inline const Evaluate &evaluate_with_head() const {
		return *_evaluate_with_head;
	}

	BaseExpressionRef own_value;
	RuleTable sub_rules;
	RuleTable up_rules;
	RuleTable down_rules;

	virtual bool same(const BaseExpression &expr) const {
		// compare as pointers: Symbol instances are unique
		return &expr == this;
	}

	virtual hash_t hash() const {
		return hash_pair(symbol_hash, (std::uintptr_t)this);
	}

	virtual std::string fullform() const {
		return name();
	}

	const char *name() const {
		return _name;
	}

	inline BaseExpressionRef evaluate_symbol() const {
		return own_value;
	}

	void add_down_rule(const RuleRef &rule);

	void add_sub_rule(const RuleRef &rule);

	virtual bool match(const BaseExpression &expr) const {
		return same(expr);
	}

	virtual BaseExpressionRef replace_all(const Match &match) const;

	inline void set_matched_value(const MatchId &id, const BaseExpressionRef &value) const {
		_match_id = id;
		_match_value = value;
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

	inline BaseExpressionRef matched_value(const MatchId &id) const {
		if (_match_id == id) {
			return _match_value;
		} else {
			return BaseExpressionRef();
		}
	}

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

	void add_rule(const BaseExpressionRef &lhs, const BaseExpressionRef &rhs);

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
		BaseExpressionRef form;

		const BaseExpression *expr = result ? result.get() : this;
		switch (expr->type()) {
			case ExpressionType:
				form = static_cast<const Expression*>(expr)->evaluate_expression(evaluation);
				break;
			case SymbolType:
				form = static_cast<const Symbol*>(expr)->evaluate_symbol();
				break;
			default:
				return result;
		}

		if (form) {
			result = form;
		} else {
			break;
		}
	}

	return result;
}

inline BaseExpressionRef BaseExpression::evaluate_or_identity(
    const Evaluation &evaluation) const {

    const BaseExpressionRef result = evaluate(evaluation);
    return result ? result : BaseExpressionRef(this);
}

inline Symbol *BaseExpression::as_symbol() const {
    return const_cast<Symbol*>(static_cast<const Symbol*>(this));
}

template<typename F>
inline BaseExpressionRef scope(
	Symbol *symbol,
	const BaseExpressionRef &value,
	const F &f) {

	BaseExpressionRef result;

	const BaseExpressionRef safed_own_value = symbol->own_value;
	try {
		symbol->own_value = value;
		result = f();
	} catch(...) {
		symbol->own_value = safed_own_value;
		throw;
	}
	symbol->own_value = safed_own_value;

	return result;
}

#endif //CMATHICS_SYMBOL_H_H
