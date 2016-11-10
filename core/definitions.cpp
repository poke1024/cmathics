#include <assert.h>

#include "definitions.h"
#include "evaluation.h"
#include "pattern.h"

Symbol::Symbol(Definitions *definitions, const char *name, PatternSymbol pattern) :
    BaseExpression(SymbolType),
    _name(name),
    _linked_variable(nullptr),
    attributes(Attributes::None),
    pattern_symbol(pattern) {

	// initialise a definition entry

    /*const auto empty_list = definitions->empty_list();
    own_values = empty_list;
    sub_values = empty_list;
    up_values = empty_list;
    down_values = empty_list;
    n_values = empty_list;
    format_values = empty_list;
    default_values = empty_list;
    messages = empty_list;
    options = empty_list;*/
}

BaseExpressionRef Symbol::evaluate() {
    // return NULL if nothing changed
    BaseExpressionRef result;

    // try all the own values until one applies
    /*for (auto leaf : own_values->_leaves) {
        result = replace(leaf);
        if (result != NULL)
            break;
    }*/

    return result;
}

void Symbol::add_down_rule(const Rule &rule) {
    down_rules.push_back(rule);
}

BaseExpressionRef Symbol::replace_all(const Match &match) const {
	if (match.id() == _match_id) {
		return _match_value;
	} else {
		return BaseExpressionRef();
	}
}


Definitions::Definitions() {
    // construct common `List[]` for bootstrapping
    auto list = new_symbol("System`List");
	_empty_list = expression(list, {});
	_list = list;

    // add important system symbols
    auto sequence = SymbolRef(new ::Sequence(this));
    add_internal_symbol(sequence);
    _sequence = sequence;

    _true = new_symbol("System`True");
    _false = new_symbol("System`False");

    // bootstrap pattern matching symbols
    add_internal_symbol(SymbolRef(new Blank(this)));
    add_internal_symbol(SymbolRef(new BlankSequence(this)));
    add_internal_symbol(SymbolRef(new BlankNullSequence(this)));
    add_internal_symbol(SymbolRef(new Pattern(this)));
    add_internal_symbol(SymbolRef(new Alternatives(this)));
    add_internal_symbol(SymbolRef(new Repeated(this)));

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

SymbolRef Definitions::new_symbol(const char *name) {
    assert(_definitions.find(name) == _definitions.end());
    auto symbol = SymbolRef(new Symbol(this, name));
    _definitions[name] = symbol;
    return symbol;
}

SymbolRef Definitions::lookup(const char *name) {
    auto it = _definitions.find(name);

    if (it == _definitions.end()) {
        return new_symbol(name); // FIXME do we really want this?
    } else {
        return it->second;
    }
}

void Definitions::add_internal_symbol(const SymbolRef &symbol) {
    _definitions[symbol->_name] = symbol;
}
