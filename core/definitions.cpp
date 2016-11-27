#include <assert.h>

#include "types.h"
#include "expression.h"
#include "evaluation.h"
#include "pattern.h"
#include "builtin.h"

inline DefinitionsPos get_definitions_pos(
	const BaseExpressionRef &pattern,
	const Symbol *symbol) {

	// see core/definitions.py:get_tag_position()
	if (pattern == symbol) {
		return DefinitionsPos::Own;
	} else if (pattern->type() != ExpressionType) {
		return DefinitionsPos::None;
	} else if (pattern->head() == symbol) {
		return DefinitionsPos::Down;
	} else if (pattern->lookup_name() == symbol) {
		return DefinitionsPos::Sub;
	} else {
		return DefinitionsPos::None;
	}
}

Symbols::Symbols(Definitions &definitions) :
	#define SYMBOL(SYMBOLNAME) \
		SYMBOLNAME(definitions.system_symbol(#SYMBOLNAME, Symbol ## SYMBOLNAME)),
	#include "system_symbols.h"
	#undef SYMBOL
	_dummy() {
}

Symbol::Symbol(const char *name, ExtendedType symbol) :
    BaseExpression(symbol),
    _linked_variable(nullptr),
    _replacement(nullptr) {

	const size_t n = snprintf(
		_short_name, sizeof(_short_name), "%s", name);
	if (n >= sizeof(_short_name)) {
		_name = new char[strlen(name) + 1];
		strcpy(_name, name);
	} else {
		_name = _short_name;
	}

	set_attributes(Attributes::None);
}

Symbol::~Symbol() {
	if (_name != _short_name) {
		delete[] _name;
	}
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
	if (i != rules.end() && (*i)->pattern->same(rule->pattern)) {
		*i = rule;
	} else {
		rules.insert(i, rule);
	}
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

void Symbol::add_rule(const BaseExpressionRef &lhs, const BaseExpressionRef &rhs) {
	switch (get_definitions_pos(lhs, this)) {
		case DefinitionsPos::None:
			break;
		case DefinitionsPos::Own:
			own_value = rhs;
			break;
		case DefinitionsPos::Down:
			add_down_rule(std::make_shared<RewriteRule>(lhs, rhs));
			break;
		case DefinitionsPos::Sub:
			add_sub_rule(std::make_shared<RewriteRule>(lhs, rhs));
			break;
	}
}

void Symbol::add_rule(const RuleRef &rule) {
	switch (get_definitions_pos(rule->pattern, this)) {
		case DefinitionsPos::None:
			break;
		case DefinitionsPos::Own:
			own_value = rule->rhs();
			break;
		case DefinitionsPos::Down:
			add_down_rule(rule);
			break;
		case DefinitionsPos::Sub:
			add_sub_rule(rule);
			break;
	}
}

Definitions::Definitions() : _symbols(*this) {
}

SymbolRef Definitions::new_symbol(const char *name, ExtendedType type) {
    assert(_definitions.find(name) == _definitions.end());
    const SymbolRef symbol = Heap::Symbol(name, type);
    _definitions[SymbolKey(symbol)] = symbol;
    return symbol;
}

Symbol *Definitions::system_symbol(const char *name, ExtendedType type) {
	std::ostringstream fullname;
	fullname << "System`";
	if (strncmp(name, "State", 5) == 0) {
		fullname << "$";
		fullname << name + 5;
	} else {
		fullname << name;
	}
	return new_symbol(fullname.str().c_str(), type).get();
}

SymbolRef Definitions::lookup(const char *name) {
    auto it = _definitions.find(SymbolKey(name));

    if (it == _definitions.end()) {
        return new_symbol(name); // FIXME do we really want this?
    } else {
        return it->second;
    }
}

