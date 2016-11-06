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

typedef std::function<BaseExpressionRef(const ExpressionRef &expr, Evaluation &evaluation)> Rule;

typedef std::vector<Rule> Rules;

class Definitions;

class Symbol : public BaseExpression {
protected:
    friend class Definitions;

    const std::string _name;

    mutable MatchId _match_id;
    mutable BaseExpressionRef _match_value;
    mutable const Symbol *_linked_variable;

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
};

template<int M, int N>
struct unpack_symbols {
    void operator()(const Symbol *symbol, typename BaseExpressionTuple<M>::type &t) {
        // symbols are ordered in reverse order of their original appearance in the pattern.
        // we reverse this again, so they appear in the pattern's order in the tuple t.
        assert(symbol != nullptr);
        std::get<N - 1>(t) = symbol->matched_value();
        unpack_symbols<M, N - 1>()(symbol->linked_variable(), t);
    }
};

template<int M>
struct unpack_symbols<M, 0> {
    void operator()(const Symbol *symbol, typename BaseExpressionTuple<M>::type &t) {
        assert(symbol == nullptr);
    }
};

template<int N>
inline typename BaseExpressionTuple<N>::type Match::get() const {
    typename BaseExpressionTuple<N>::type t;
    unpack_symbols<N, N>()(_variables, t);
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

    /*template<typename F, typename... Vars>
    inline std::function<BaseExpressionRef(void)> build_call_to(F func, Vars... vars) {
        return build_call([this](const char *name) {
            // built-in patterns like Most[x_List] always map their variables (e.g. x)
            // to System, so place the parameter names received here in System as well.
            const std::string full_name(std::string("System`") + name);
            return this->lookup(full_name.c_str());
        }, func)(vars...);
    }*/
};

#endif
