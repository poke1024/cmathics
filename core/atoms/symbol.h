#ifndef CMATHICS_SYMBOL_H_H
#define CMATHICS_SYMBOL_H_H

#include "../types.h"
#include "../rule.h"

#include <string>
#include <vector>
#include <map>
#include <bitset>

#include <symengine/symbol.h>
#include <symengine/constants.h>

#include "../attributes.h"

class Definitions;

struct EvaluateVTable;

class Messages : public HeapObject<Messages> {
private:
	Rules m_rules;

public:
	void add(const SymbolRef &name, const char *tag, const char *text, const Evaluation &evaluation);

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

    void set_attributes(
        Attributes attributes,
        const Evaluation &evaluation);

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
	SymbolPtr m_symbol;

	Attributes m_attributes;
	DispatchableAttributes m_dispatch;

	UnsafeBaseExpressionRef m_own_value;

	SymbolRulesRef m_rules;
	bool m_copy_on_write;

protected:
	friend class Symbol;

	void clear_attributes();

public:
	inline SymbolState(SymbolPtr symbol) : m_symbol(symbol), m_copy_on_write(false) {
	}

	inline void set_as_copy_of(const SymbolState &state) {
		assert(m_symbol == state.m_symbol);
		m_attributes = state.m_attributes;
		m_dispatch = state.m_dispatch;
		m_own_value = state.m_own_value;
		m_rules = state.m_rules;
		m_copy_on_write = true;
	}

	inline void mark_as_copy() {
		m_copy_on_write = true;
	}

	inline SymbolState(const SymbolState &state) : m_symbol(state.m_symbol) {
		set_as_copy_of(state);
	}

    void clear();

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

	inline void add_down_rule(const RuleRef &rule, const Evaluation &evaluation) {
		mutable_rules()->down_rules.add(rule, evaluation);
	}

	inline const Rules *down_rules() const {
		return m_rules ? &m_rules->down_rules : nullptr;
	}

    inline void add_up_rule(const RuleRef &rule, const Evaluation &evaluation) {
        mutable_rules()->up_rules.add(rule, evaluation);
    }

	inline const Rules *up_rules() const {
		return m_rules ? &m_rules->up_rules : nullptr;
	}

	inline void add_sub_rule(const RuleRef &rule, const Evaluation &evaluation) {
		mutable_rules()->sub_rules.add(rule, evaluation);
	}

	inline const Rules *sub_rules() const {
		return m_rules ? &m_rules->sub_rules : nullptr;
	}

	void add_rule(
		BaseExpressionPtr lhs,
		BaseExpressionPtr rhs,
		const Evaluation &evaluation);

	void add_rule(
		const RuleRef &rule,
		const Evaluation &evaluation);

    void add_format(
        const RuleRef &rule,
        const SymbolRef &form,
        const Evaluation &evaluation);

	bool has_format(
		const BaseExpressionRef &lhs,
		const Evaluation &evaluation) const;

	inline Attributes attributes() const {
		return m_attributes;
	}

    inline bool has_attributes(Attributes attributes) const {
        return m_attributes & attributes;
    }

    void clear_attributes(const Evaluation &evaluation);

	void add_attributes(Attributes attributes, const Evaluation &evaluation);

	void remove_attributes(Attributes attributes, const Evaluation &evaluation);

	BaseExpressionRef dispatch(
		const Expression *expr,
		SliceCode slice_code,
		const Slice &slice,
		const Evaluation &evaluation) const;
};

class Symbol : public BaseExpression, public PoolObject<Symbol> {
protected:
	friend class Definitions;

	char _short_name[32];
	char *_name;

protected:
	//friend class EvaluationContext;
	friend class ParallelTask;
	friend const SymbolState &symbol_state(const Symbol *symbol);
	friend SymbolState &mutable_symbol_state(const Symbol *symbol);

	mutable optional<SymbolState> m_builtin_state;
	mutable TaskLocalStorage<SymbolState> m_state;

protected:
    virtual SymbolicFormRef instantiate_symbolic_form(const Evaluation &evaluation) const;

public:
    static constexpr Type Type = SymbolType;

	Symbol(const char *name, ExtendedType symbol = ExtendedType(S::GENERIC));

	~Symbol();

	virtual std::string debugform() const;

	inline const SymbolState &state() const {
		return m_state.get();
	}

	inline SymbolState &mutable_state() const {
		return m_state.modify();
	}

	inline void freeze_as_builtin() const {
		m_builtin_state.emplace(SymbolState(state()));
		m_state.modify().mark_as_copy();
	}

    void reset_user_definitions() const;

    virtual BaseExpressionPtr head(const Symbols &symbols) const final;

	virtual inline bool same_indeed(const BaseExpression &expr) const final {
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
        const Evaluation &evaluation) const;

	inline BaseExpressionRef evaluate_symbol() const {
		return state().own_value();
	}

	void add_message(
		const char *tag,
		const char *text,
		const Evaluation &evaluation) const;

	StringRef lookup_message(
		const Expression *message,
		const Evaluation &evaluation) const;

	virtual bool match(const BaseExpression &expr) const {
		return same(expr);
	}

	virtual BaseExpressionRef replace_all(const MatchRef &match, const Evaluation &evaluation) const;

	virtual BaseExpressionRef replace_all(const ArgumentsMap &replacement, const Evaluation &evaluation) const;

	virtual void sort_key(SortKey &key, const Evaluation &evaluation) const final {
        MonomialMap map;
        map[SymbolKey(SymbolRef(this))] = 1;
		return key.construct(
			is_numeric() ? 1 : 2, 2, std::move(map), 0, name(), 1);
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

    virtual MatchSize string_match_size() const final;
};

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
	SymbolState &state = symbol->mutable_state();
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

inline SymbolPtr BaseExpression::lookup_name() const {
    switch (type()) {
        case SymbolType:
            return static_cast<const Symbol*>(this);
        case ExpressionType:
            return as_expression()->lookup_name();
        default:
            return nullptr;
    }
}

#include "../pattern/arguments.tcc"

#endif //CMATHICS_SYMBOL_H_H
