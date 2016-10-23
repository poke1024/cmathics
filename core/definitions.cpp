#include <assert.h>

#include "definitions.h"
#include "pattern.h"

// initialise a definition entry
Symbol::Symbol(Definitions *definitions, const char *name) : _name(name), _matched_value(nullptr) {
    const auto empty_list = definitions->empty_list();
    own_values = empty_list;
    sub_values = empty_list;
    up_values = empty_list;
    down_values = empty_list;
    n_values = empty_list;
    format_values = empty_list;
    default_values = empty_list;
    messages = empty_list;
    options = empty_list;

    sub_code = nullptr;
    up_code = nullptr;
    down_code = nullptr;
}

BaseExpression *Symbol::evaluate() {
    // return NULL if nothing changed
    BaseExpression* result = NULL;

    // try all the own values until one applies
    for (auto leaf : own_values->_leaves) {
        result = replace(leaf);
        if (result != NULL)
            break;
    }
    return result;
}


Definitions::Definitions() {
    // construct common `List[]` for bootstrapping
    _empty_list = nullptr;
    auto list = new_symbol("System`List");
    _empty_list = new Expression(list, {});
    _empty_list->ref();

    // add important system symbols
    add_internal_symbol(new Sequence(this));

    // bootstrap pattern matching symbols
    add_internal_symbol(new Blank(this));
    add_internal_symbol(new BlankSequence(this));
    add_internal_symbol(new BlankNullSequence(this));
    add_internal_symbol(new Pattern(this));
    add_internal_symbol(new Alternatives(this));
    add_internal_symbol(new Repeated(this));

    /*new_symbol("System`RepeatedNull", RepeatedNull);
    new_symbol("System`Except", Except);
    new_symbol("System`Longest", Longest);
    new_symbol("System`Shortest", Shortest);
    new_symbol("System`OptionsPattern", OptionsPattern);
    new_symbol("System`PatternSequence", PatternSequence);
    new_symbol("System`OrderlessPatternSequence", OrderlessPatternSequence);
    new_symbol("System`Verbatim", Verbatim);
    new_symbol("System`HoldPattern", HoldPattern);
    new_symbol("System`KeyValuePattern", KeyValuePattern);
    new_symbol("System`Condition", Condition);
    new_symbol("System`PatternTest", PatternTest);
    new_symbol("System`Optional", Optional);*/
}

Symbol *Definitions::new_symbol(const char *name) {
    assert(_definitions.find(name) == _definitions.end());
    Symbol *symbol = new Symbol(this, name);
    _definitions[name] = symbol;
    return symbol;
}

Symbol *Definitions::lookup(const char *name) const {
    auto it = _definitions.find(name);
    if (it == _definitions.end()) {
        return nullptr;
    } else {
        return it->second;
    }
}

void Definitions::add_internal_symbol(Symbol *symbol) {
    _definitions[symbol->_name] = symbol;
    symbol->ref();
}
