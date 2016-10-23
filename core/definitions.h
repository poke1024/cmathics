#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "types.h"
#include "expression.h"

#include <string>
#include <vector>
#include <map>


// c functions used for builtin evaluation
typedef BaseExpressionPtr (CFunction)(const Expression*);


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
};


class Definitions;


typedef uint64_t match_id_t;


class Symbol : public BaseExpression {
protected:
    friend class Definitions;

    const std::string _name;

    mutable BaseExpressionPtr _matched_value;

    Symbol(Definitions *new_definitions, const char *name);

public:
    Expression* own_values;
    Expression* sub_values;
    Expression* up_values;
    Expression* down_values;
    Expression* n_values;
    Expression* format_values;
    Expression* default_values;
    Expression* messages;
    Expression* options;
    CFunction *sub_code;
    CFunction *up_code;
    CFunction *down_code;
    Attributes attributes;

    virtual Type type() const {
        return SymbolType;
    }

    virtual bool same(const BaseExpression *expr) const {
        return expr == this; // compare as pointers: symbols are unique
    }

    virtual hash_t hash() const {
        return hash_pair(symbol_hash, (std::uintptr_t)this);
    }

    virtual std::string fullform() const {
        return _name;
    }

    virtual BaseExpression *evaluate();

    virtual Match match(const BaseExpression *expr) const {
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
    std::map<std::string,Symbol*> _definitions;
    Expression *_empty_list;

    void add_internal_symbol(Symbol *symbol);

public:
    Definitions();

    Symbol *new_symbol(const char *name);
    Symbol *lookup(const char *name) const;

    inline Expression *empty_list() const {
        return _empty_list;
    }
};

#endif
