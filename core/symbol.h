#ifndef CMATHICS_SYMBOL_H_H
#define CMATHICS_SYMBOL_H_H

#include "types.h"

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
	HoldAll = 1 << 11,
	HoldAllComplete = 1 << 12,
	// evaluation nhold attributes
	NHoldFirst = 1 << 13,
	NHholdRest = 1 << 14,
	NHoldAll = 1 << 15,
	// misc attributes
	SequenceHold = 1 << 16,
	Temporary = 1 << 17,
	Stub = 1 << 18
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
	return static_cast<Attributes>(static_cast<attributes_bitmask_t>(x) & static_cast<attributes_bitmask_t>(y));
}

typedef std::function<BaseExpressionRef(const ExpressionRef &expr, const Evaluation &evaluation)> Rule;

typedef std::vector<Rule> Rules;

class Definitions;

enum class PatternSymbol : uint8_t {
	None,
	Blank,
	BlankSequence,
	BlankNullSequence,
	Pattern
};

class Symbol : public BaseExpression {
protected:
	friend class Definitions;

	const std::string _name;

	mutable MatchId _match_id;
	mutable BaseExpressionRef _match_value;
	mutable const Symbol *_linked_variable;

public:
	Symbol(Definitions *new_definitions, const char *name, PatternSymbol pattern = PatternSymbol::None);

	/*Expression* own_values;
	Expression* sub_values;
	Expression* up_values;
	Expression* down_values;
	Expression* n_values;
	Expression* format_values;
	Expression* default_values;
	Expression* messages;
	Expression* options;*/

	Rules sub_rules;
	Rules up_rules;
	Rules down_rules;

	Attributes attributes;
	const PatternSymbol pattern_symbol;

	virtual bool same(const BaseExpression &expr) const {
		// compare as pointers: Symbol instances are unique
		return &expr == this;
	}

	virtual hash_t hash() const {
		return hash_pair(symbol_hash, (std::uintptr_t)this);
	}

	virtual std::string fullform() const {
		return _name;
	}

	const std::string &name() const {
		return _name;
	}

	virtual BaseExpressionRef evaluate();

	void add_down_rule(const Rule &rule);

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

	inline void link_variable(const Symbol *symbol) const {
		_linked_variable = symbol;
	}

	inline const Symbol *linked_variable() const {
		return _linked_variable;
	}

	inline void set_attributes(Attributes a) {
		attributes = a;
	}
};

template<int M, int N>
struct unpack_symbols {
	void operator()(const Symbol *symbol, typename BaseExpressionTuple<M>::type &t) {
		// symbols are already ordered in the order of their (first) appearance in the original pattern.
		assert(symbol != nullptr);
		std::get<N>(t) = symbol->matched_value();
		unpack_symbols<M, N + 1>()(symbol->linked_variable(), t);
	}
};

template<int M>
struct unpack_symbols<M, M> {
	void operator()(const Symbol *symbol, typename BaseExpressionTuple<M>::type &t) {
		assert(symbol == nullptr);
	}
};

template<int N>
inline typename BaseExpressionTuple<N>::type Match::get() const {
	typename BaseExpressionTuple<N>::type t;
	unpack_symbols<N, 0>()(_variables, t);
	return t;
};

class Sequence : public Symbol {
public:
	Sequence(Definitions *definitions) : Symbol(definitions, "System`Sequence") {
	}

	virtual bool is_symbol_sequence() const {
		return true;
	}
};

#endif //CMATHICS_SYMBOL_H_H
