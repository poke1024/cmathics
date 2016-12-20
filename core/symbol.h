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

class FastLeafSequence;
class SlowLeafSequence;
class AsciiCharacterSequence;
class SimpleCharacterSequence;
class ComplexCharacterSequence;

class PatternMatcherSize {
private:
	MatchSize m_from_here;
	MatchSize m_from_next;

public:
	inline PatternMatcherSize() {
	}

	inline PatternMatcherSize(
			const MatchSize &from_here,
			const MatchSize &from_next) :
			m_from_here(from_here),
			m_from_next(from_next) {
	}

	inline const MatchSize &from_here() const {
		return m_from_here;
	}

	inline const MatchSize &from_next() const {
		return m_from_next;
	}
};

constexpr size_t log2(size_t n) {
	// kudos to https://hbfs.wordpress.com/2016/03/22/log2-with-c-metaprogramming/
	return (n < 2) ? 1 : 1 + log2(n / 2);
}

inline std::size_t SymbolHash::operator()(const Symbol *symbol) const {
	constexpr int bits = log2(sizeof(Symbol));
	return uintptr_t(symbol) >> bits;
}

class CompiledVariables {
private:
	VariableMap<index_t> m_indices;
	std::list<SymbolRef> m_symbols;

protected:
	friend class PatternFactory;

	index_t lookup_slot(const SymbolRef &variable) {
		const auto i = m_indices.find(variable.get());
		if (i != m_indices.end()) {
			return i->second;
		} else {
			m_symbols.push_back(variable);
			const index_t index = m_indices.size();
			m_indices[variable.get()] = index;
			return index;
		}
	}

public:
	inline index_t find(const Symbol *variable) const {
		const auto i = m_indices.find(variable);
		if (i != m_indices.end()) {
			return i->second;
		} else {
			return -1;
		}
	}

	inline size_t size() const {
		return m_indices.size();
	}

	inline bool empty() const {
		return m_indices.empty();
	}
};

class PatternMatcher {
protected:
	std::atomic<size_t> m_ref_count;
	PatternMatcherSize m_size;
	CompiledVariables m_variables;

public:
	inline void set_size(const PatternMatcherSize &size) {
		m_size = size;
	}

	inline void set_variables(const CompiledVariables &variables) {
		m_variables = variables;
	}

	inline const CompiledVariables &variables() const {
		return m_variables;
	}

	inline PatternMatcher() : m_ref_count(0) {
	}

	virtual ~PatternMatcher() {
	}

	inline bool might_match(size_t size) const {
		return m_size.from_here().contains(size);
	}

	inline optional<size_t> fixed_size() const {
		return m_size.from_here().fixed_size();
	}

	virtual index_t match(
		const FastLeafSequence &sequence,
		index_t begin,
		index_t end) const = 0;

	virtual index_t match(
		const SlowLeafSequence &sequence,
		index_t begin,
		index_t end) const = 0;

	virtual index_t match(
		const AsciiCharacterSequence &sequence,
		index_t begin,
		index_t end) const = 0;

	virtual index_t match(
		const SimpleCharacterSequence &sequence,
		index_t begin,
		index_t end) const = 0;

	virtual index_t match(
		const ComplexCharacterSequence &sequence,
		index_t begin,
		index_t end) const = 0;

	index_t match(
		MatchContext &context,
		const String *string,
		index_t begin,
		index_t end) const;

	friend void intrusive_ptr_add_ref(PatternMatcher *matcher);
	friend void intrusive_ptr_release(PatternMatcher *matcher);
};


inline void intrusive_ptr_add_ref(PatternMatcher *matcher) {
	matcher->m_ref_count++;
}

inline void intrusive_ptr_release(PatternMatcher *matcher) {
	if (--matcher->m_ref_count == 0) {
		delete matcher;
	}
}

#endif //CMATHICS_SYMBOL_H_H
