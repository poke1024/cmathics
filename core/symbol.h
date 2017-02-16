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

struct EvaluateVTable;

class Messages : public Shared<Messages, SharedHeap> {
private:
	Rules m_rules;

public:
	void add(const SymbolRef &name, const char *tag, const char *text, const Definitions &definitions);

	StringRef lookup(const Expression *message, const Evaluation &evaluation) const;
};

typedef QuasiConstSharedPtr<Messages> MessagesRef;

struct SymbolRules {
	inline SymbolRules() {
	}

	inline SymbolRules(const SymbolRules &rules) :
		sub_rules(rules.sub_rules),
		up_rules(rules.up_rules),
		down_rules(rules.down_rules),
        format_values(rules.format_values),
		messages(rules.messages) {
	}

	Rules sub_rules;
	Rules up_rules;
	Rules down_rules;
	FormatRules format_values;

    MessagesRef messages;

	/*
	Expression* n_values;
	Expression* default_values;
	Expression* options;
	*/
};

typedef std::shared_ptr<SymbolRules> SymbolRulesRef;

class Evaluate;

typedef uint64_t DispatchableAttributes;

class SymbolState {
	// only ever to be accessed by one single thread.

private:
	const Symbol * const m_symbol;

	Attributes m_attributes;
	DispatchableAttributes m_dispatch;

	UnsafeBaseExpressionRef m_own_value;

	SymbolRulesRef m_rules;
	bool m_copy_on_write;

public:
	inline SymbolState(const Symbol *symbol) : m_symbol(symbol), m_copy_on_write(false) {
	}

	inline SymbolState(const SymbolState &state) : m_symbol(state.m_symbol) {
		m_attributes = state.m_attributes;
		m_dispatch = state.m_dispatch;
		m_own_value = state.m_own_value;
		m_rules = state.m_rules;
		m_copy_on_write = true;
	}

	inline const UnsafeBaseExpressionRef &own_value() const {
		return m_own_value;
	}

	inline void set_own_value(const UnsafeBaseExpressionRef &value) {
		m_own_value = value;
	}

	inline SymbolRules *mutable_rules() {
		if (!m_rules) {
			m_rules = std::make_shared<SymbolRules>();
		} else if (m_copy_on_write) {
			m_rules = std::make_shared<SymbolRules>(*m_rules.get());
			m_copy_on_write = false;
		}
		return m_rules.get();
	}

	inline SymbolRules *rules() const {
		return m_rules.get();
	}

	inline void add_down_rule(const RuleRef &rule) {
		mutable_rules()->down_rules.add(rule);
	}

    inline void add_up_rule(const RuleRef &rule) {
        mutable_rules()->up_rules.add(rule);
    }

	inline void add_sub_rule(const RuleRef &rule) {
		mutable_rules()->sub_rules.add(rule);
	}

	void add_rule(BaseExpressionPtr lhs, BaseExpressionPtr rhs);

	void add_rule(const RuleRef &rule);

    void add_format(
        const RuleRef &rule,
        const SymbolRef &form,
        const Definitions &definitions);

	template<typename Expression>
	BaseExpressionRef format(
		const Expression *expr,
		const SymbolRef &form,
		const Evaluation &evaluation) const;

	void set_attributes(Attributes attributes);

	inline bool has_attributes(Attributes attributes) const {
		return m_attributes & attributes;
	}

	BaseExpressionRef dispatch(
		const Expression *expr,
		SliceCode slice_code,
		const Slice &slice,
		const Evaluation &evaluation) const;
};

class EvaluationContext {
private:
	EvaluationContext * const m_parent;

	EvaluationContext *m_saved_context;

	SymbolStateMap m_symbols;

	static thread_local EvaluationContext *s_current;

public:
	inline EvaluationContext(EvaluationContext *parent) :
		m_parent(parent), m_symbols(Pool::symbol_state_map_allocator()) {

		EvaluationContext *&current = s_current;
		m_saved_context = current;
		current = this;
	}

	inline ~EvaluationContext() {
		s_current = m_saved_context;
	}

	static inline EvaluationContext *current() {
		return s_current;
	}

	inline SymbolState &state(const Symbol *symbol);
};

class Symbol : public BaseExpression {
protected:
	friend class Definitions;

	char _short_name[32];
	char *_name;

protected:
	friend class EvaluationContext;

	mutable SymbolState m_master_state;

protected:
    virtual SymbolicFormRef instantiate_symbolic_form() const;

public:
    static constexpr Type Type = SymbolType;

	Symbol(const char *name, ExtendedType symbol = ExtendedType(S::GENERIC));

	~Symbol();

	virtual std::string debugform() const;

	inline SymbolState &state() const {
		EvaluationContext *context = EvaluationContext::current();
		if (context == nullptr) {
			return m_master_state;
		} else {
			return context->state(this);
		}
	}

	virtual BaseExpressionPtr head(const Symbols &symbols) const final;

	virtual inline bool same(const BaseExpression &expr) const final {
		// compare as pointers: Symbol instances are unique
		return &expr == this;
	}

    virtual inline tribool equals(const BaseExpression &expr) const final {
        if (&expr == this) {
            return true;
        } else {
            return undecided;
        }
    }

	virtual hash_t hash() const {
		return hash_pair(symbol_hash, (std::uintptr_t)this);
	}

    virtual std::string format(const SymbolRef &form, const Evaluation &evaluation) const {
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

    virtual BaseExpressionRef make_boxes(
        BaseExpressionPtr form,
        const Evaluation &evaluation) const {

        return Pool::String(short_name());
    }

	inline BaseExpressionRef evaluate_symbol() const {
		return state().own_value();
	}

	void add_message(
		const char *tag,
		const char *text,
		const Definitions &definitions) const;

	StringRef lookup_message(
		const Expression *message,
		const Evaluation &evaluation) const;

	virtual bool match(const BaseExpression &expr) const {
		return same(expr);
	}

	virtual BaseExpressionRef replace_all(const MatchRef &match) const;

	virtual SortKey sort_key() const final {
        MonomialMap map(Pool::monomial_map_allocator());
        map[SymbolKey(SymbolRef(this))] = 1;
		return SortKey(is_numeric() ? 1 : 2, 2, std::move(map), 0, name(), 1);
	}

	virtual bool is_numeric() const final {
		switch (symbol()) {
			case S::Pi:
			case S::E:
			case S::EulerGamma:
			case S::GoldenRatio:
			case S::MachinePrecision:
			case S::Catalan:
				return true;
			default:
				return false;
		}
	}

	virtual std::string boxes_to_text(const Evaluation &evaluation) const {
		return name();
	}
};

template<typename Expression>
BaseExpressionRef SymbolState::format(
	const Expression *expr,
	const SymbolRef &form,
	const Evaluation &evaluation) const {

	if (expr->is_expression() &&
	    expr->as_expression()->head()->is_expression()) {
		// expr is of the form f[...][...]
		return BaseExpressionRef();
	}

	const Symbol * const name = expr->lookup_name();
	const SymbolRules * const rules = name->state().rules();
	if (rules) {
        const optional<BaseExpressionRef> result =
            rules->format_values.apply(expr, form, evaluation);
        if (result && *result) {
            return (*result)->evaluate_or_copy(evaluation);
        }
    }

    return BaseExpressionRef();
}

inline SymbolState &EvaluationContext::state(const Symbol *symbol) {
	const auto i = m_symbols.find(symbol);
	if (i != m_symbols.end()) {
		return i->second;
	} else {
		const SymbolState &state = m_parent ?
			m_parent->state(symbol) :
			symbol->m_master_state;
		return m_symbols.emplace(SymbolRef(symbol), state).first->second;
	}
}

inline int SymbolKey::compare(const SymbolKey &key) const {
    if (_symbol) {
        if (key._symbol) {
            return strcmp(_symbol->name(), key._symbol->name());
        } else {
            return strcmp(_symbol->name(), key._name);
        }
    } else {
        if (key._symbol) {
            return strcmp(_name, key._symbol->name());
        } else {
            return strcmp(_name, key._name);
        }
    }
}

inline bool SymbolKey::operator==(const SymbolKey &key) const {
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

inline const char *SymbolKey::c_str() const {
    return _name ? _name : _symbol->name();
}

inline BaseExpressionRef BaseExpression::evaluate(
	const Evaluation &evaluation) const {

    UnsafeBaseExpressionRef result;

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
	            break;
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

#if 1
	SymbolState &state = symbol->state();
	const BaseExpressionRef old_value(state.own_value());
	state.set_own_value(value);

	try {
		const BaseExpressionRef result = f();
		state.set_own_value(old_value);
		return result;
	} catch(...) {
		state.set_own_value(old_value);
		throw;
	}
#else
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
#endif
}

template<typename F>
inline auto scoped(
	Symbol *symbol,
	const F &f) {

	return [symbol, &f] (BaseExpressionRef &&value) {
		return scope(symbol, std::move(value), f);
	};
}

constexpr size_t log2(size_t n) {
	// kudos to https://hbfs.wordpress.com/2016/03/22/log2-with-c-metaprogramming/
	return (n < 2) ? 1 : 1 + log2(n / 2);
}

inline std::size_t SymbolHash::operator()(const Symbol *symbol) const {
	constexpr int bits = log2(sizeof(Symbol));
	return uintptr_t(symbol) >> bits;
}

inline std::size_t SymbolHash::operator()(const SymbolRef &symbol) const {
	return (*this)(symbol.get());
}

inline SlotDirective CompiledArguments::operator()(const BaseExpressionRef &item) const {
	if (item->is_symbol()) {
		const index_t index = m_variables.find(
			static_cast<const Symbol*>(item.get()));
		if (index >= 0) {
			return SlotDirective::slot(index);
		} else {
			return SlotDirective::copy();
		}
	} else {
        if (item->is_expression()) {
            const Expression * const expr = item->as_expression();

            if (expr->head()->symbol() == S::OptionValue && expr->size() == 1) {
                const BaseExpressionRef &leaf = expr->n_leaves<1>()[0];
                if (leaf->is_symbol()) {
                    return SlotDirective::option_value(leaf->as_symbol());
                }
            }
        }

		return SlotDirective::descend();
	}
}

inline const Symbol *BaseExpression::lookup_name() const {
    switch (type()) {
        case SymbolType:
            return static_cast<const Symbol*>(this);
        case ExpressionType:
            return as_expression()->lookup_name();
        default:
            return nullptr;
    }
}

#endif //CMATHICS_SYMBOL_H_H
