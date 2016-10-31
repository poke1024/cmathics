#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "types.h"
#include "expression.h"

#include <string>
#include <vector>
#include <map>


// c functions used for builtin evaluation
typedef std::function<BaseExpressionRef(const ExpressionRef &)> rule_function;

struct Attributes {
    // pattern matching attributes
    unsigned int is_orderless: 1;
    unsigned int is_flat: 1;
    unsigned int is_oneidentity: 1;
    unsigned int is_listable: 1;
    // calculus attributes
    unsigned int is_constant: 1;
    unsigned int is_numericfunction: 1;
    // rw attributes
    unsigned int is_protected: 1;
    unsigned int is_locked: 1;
    unsigned int is_readprotected: 1;
    // evaluation hold attributes
    unsigned int is_holdfirst: 1;
    unsigned int is_holdrest: 1;
    unsigned int is_holdall: 1;
    unsigned int is_holdallcomplete: 1;
    // evaluation nhold attributes
    unsigned int is_nholdfirst: 1;
    unsigned int is_nholdrest: 1;
    unsigned int is_nholdall: 1;
    // misc attributes
    unsigned int is_sequencehold: 1;
    unsigned int is_temporary: 1;
    unsigned int is_stub: 1;

    inline void clear() {
        memset(this, 0, sizeof(*this));
    }
};

class Rule;

typedef std::unique_ptr<Rule> RuleRef;

typedef std::vector<RuleRef> Rules;

class Definitions;

class Symbol : public BaseExpression {
protected:
    friend class Definitions;

    const std::string _name;

    mutable BaseExpressionPtr _matched_value;

public:
    Symbol(Definitions *new_definitions, const char *name);

    Expression* own_values;
    Expression* sub_values;
    Expression* up_values;
    Expression* down_values;
    Expression* n_values;
    Expression* format_values;
    Expression* default_values;
    Expression* messages;
    Expression* options;

    Rules sub_rules;
    Rules up_rules;
    Rules down_rules;

    Attributes attributes;

    virtual Type type() const {
        return SymbolType;
    }

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

    virtual Match match(const BaseExpression &expr) const {
        return Match(same(expr));
    }

    inline void set_matched_value(BaseExpressionPtr value) const {
        _matched_value = value;
    }

    inline void clear_matched_value() const {
        _matched_value = nullptr;
    }

    inline BaseExpressionPtr matched_value() const {
        return _matched_value;
    }

    void add_down_rule(const BaseExpressionRef &patt, rule_function func);
};

// in contrast to BaseExpressionRef, SymbolRefs are not constant. Symbols might
// change, after all, if rules are added.
typedef std::shared_ptr<Symbol> SymbolRef;

class Sequence : public Symbol {
public:
    Sequence(Definitions *definitions) : Symbol(definitions, "System`Sequence") {
    }

    virtual bool is_symbol_sequence() const {
        return true;
    }
};

class Definitions {
private:
    std::map<std::string,SymbolRef> _definitions;
    std::shared_ptr<Expression> _empty_list;
    SymbolRef _sequence;

    void add_internal_symbol(const SymbolRef &symbol);

public:
    Definitions();

    SymbolRef new_symbol(const char *name);
    SymbolRef lookup(const char *name);

    inline Expression *empty_list() const {
        return _empty_list.get();
    }

    inline const SymbolRef &sequence() const {
        return _sequence;
    }
};

#endif
