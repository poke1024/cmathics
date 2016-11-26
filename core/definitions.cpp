#include <assert.h>

#include "types.h"
#include "expression.h"
#include "evaluation.h"
#include "pattern.h"

Symbol::Symbol(Definitions *definitions, const char *name, Type symbol) :
    BaseExpression(symbol),
    _name(name),
    _linked_variable(nullptr),
    _replacement(nullptr) {

	set_attributes(Attributes::None);

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

static struct {
	bool operator()(const RuleRef &rule, const SortKey &key) const {
		return rule->key.compare(key) < 0;
	}
} CompareSortKey;

inline void insert_rule(std::vector<RuleRef> &rules, const RuleRef &rule) {
	const SortKey key = rule->key;
	const auto i = std::lower_bound(
			rules.begin(), rules.end(), key, CompareSortKey);
	rules.insert(i, rule);
}

void Symbol::add_down_rule(const RuleRef &rule) {
	const MatchSize match_size = rule->match_size();
	for (size_t code = 0; code < NumberOfSliceCodes; code++) {
		if (match_size.matches(SliceCode(code))) {
			insert_rule(down_rules[code], rule);
		}
	}
}

void Symbol::add_sub_rule(const RuleRef &rule) {
	const MatchSize match_size = rule->match_size();
	for (size_t code = 0; code < NumberOfSliceCodes; code++) {
		if (match_size.matches(SliceCode(code))) {
			insert_rule(sub_rules[code], rule);
		}
	}
}

BaseExpressionRef Symbol::replace_all(const Match &match) const {
	if (match.id() == _match_id) {
		return _match_value;
	} else {
		return BaseExpressionRef();
	}
}

void Symbol::set_attributes(Attributes a) {
	_attributes = a;
	_evaluate_with_head = EvaluateDispatch::pick(_attributes);
}

Definitions::Definitions() {
    // construct common `List[]` for bootstrapping
    auto list = new_symbol("System`List");
	_symbols.List = list.get();

    // add important system symbols
    auto sequence = SymbolRef(new ::Sequence(this));
	_symbols.Sequence = add_internal_symbol(sequence);

	_symbols.True = new_symbol("System`True", SymbolTrue).get();
	_symbols.False = new_symbol("System`False", SymbolFalse).get();
	_symbols.Null = new_symbol("System`Null").get();

    // bootstrap pattern matching symbols
	_symbols.Blank = add_internal_symbol(SymbolRef(new class Blank(this)));
	_symbols.BlankSequence = add_internal_symbol(SymbolRef(new class BlankSequence(this)));
	_symbols.BlankNullSequence = add_internal_symbol(SymbolRef(new class BlankNullSequence(this)));
	_symbols.Pattern = add_internal_symbol(SymbolRef(new class Pattern(this)));
	_symbols.Alternatives = add_internal_symbol(SymbolRef(new class Alternatives(this)));
	_symbols.Repeated = add_internal_symbol(SymbolRef(new class Repeated(this)));
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

	_symbols.Slot = new_symbol("System`Slot", SymbolSlot).get();
	_symbols.SlotSequence = new_symbol("System`SlotSequence", SymbolSequence).get();
	_symbols.Function = new_symbol("System`Function", SymbolFunction).get();
}

SymbolRef Definitions::new_symbol(const char *name, Type type) {
    assert(_definitions.find(name) == _definitions.end());
    auto symbol = SymbolRef(new Symbol(this, name, type));
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

Symbol *Definitions::add_internal_symbol(const SymbolRef &symbol) {
    _definitions[symbol->_name] = symbol;
	return symbol.get();
}
