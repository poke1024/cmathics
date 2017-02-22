#include "types.h"
#include "expression.h"
#include "evaluation.h"
#include "pattern.h"
#include "builtin.h"
#include "matcher.h"

inline DefinitionsPos get_definitions_pos(
	BaseExpressionPtr pattern,
	const Symbol *symbol) {

	// see core/definitions.py:get_tag_position()
	if (pattern == symbol) {
		return DefinitionsPos::Own;
	} else if (!pattern->is_expression()) {
		return DefinitionsPos::None;
	} else if (pattern->as_expression()->head() == symbol) {
		return DefinitionsPos::Down;
	} else if (pattern->lookup_name() == symbol) {
		return DefinitionsPos::Sub;
	} else {
        if (pattern->is_expression()) {
            return pattern->as_expression()->with_slice([symbol] (const auto &slice) {
                if (slice.type_mask() & make_type_mask(SymbolType, ExpressionType)) {
                    const size_t n = slice.size();

                    for (size_t i = 0; i < n; i++) {
                        if (slice[i]->lookup_name() == symbol) {
                            return DefinitionsPos::Up;
                        }
                    }
                }

                return DefinitionsPos::None;
            });
        } else {
            return DefinitionsPos::None;
        }
	}
}

Symbols::Symbols(Definitions &definitions) :
	#define SYMBOL(SYMBOLNAME) \
		SYMBOLNAME(definitions.system_symbol(#SYMBOLNAME, S::SYMBOLNAME)),
	#include "system_symbols.h"
	#undef SYMBOL
	_dummy() {
}

Symbol::Symbol(const char *name, ExtendedType symbol) :
    BaseExpression(symbol), m_user_state(this) {

	const size_t n = snprintf(
		_short_name, sizeof(_short_name), "%s", name);
	if (n >= sizeof(_short_name)) {
		_name = new char[strlen(name) + 1];
		strcpy(_name, name);
	} else {
		_name = _short_name;
	}

	state().set_attributes(Attributes::None);
}

Symbol::~Symbol() {
	if (_name != _short_name) {
		delete[] _name;
	}
}

BaseExpressionRef Symbol::replace_all(const MatchRef &match) const {
	const UnsafeBaseExpressionRef *value = match->get_matched_value(this);
	if (value) {
		return *value;
	} else {
		return BaseExpressionRef();
	}
}

void SymbolState::set_attributes(Attributes attributes) {
	m_attributes = attributes;
    m_dispatch = EvaluateDispatch::pick(attributes);
}

void SymbolState::add_rule(BaseExpressionPtr lhs, BaseExpressionPtr rhs) {
	switch (get_definitions_pos(lhs, m_symbol)) {
		case DefinitionsPos::None:
			break;
		case DefinitionsPos::Own:
			m_own_value = rhs;
			break;
        case DefinitionsPos::Up:
            add_up_rule(new UpRule(lhs, rhs));
            break;
		case DefinitionsPos::Down:
			add_down_rule(new DownRule(lhs, rhs));
			break;
		case DefinitionsPos::Sub:
			add_sub_rule(new SubRule(lhs, rhs));
			break;
	}
}

void SymbolState::add_rule(const RuleRef &rule) {
	switch (get_definitions_pos(rule->pattern.get(), m_symbol)) {
		case DefinitionsPos::None:
			break;
		case DefinitionsPos::Own:
			m_own_value = rule->rhs();
			break;
        case DefinitionsPos::Up:
            add_up_rule(rule);
            break;
		case DefinitionsPos::Down:
			add_down_rule(rule);
			break;
		case DefinitionsPos::Sub:
			add_sub_rule(rule);
			break;
	}
}

void SymbolState::add_format(
    const RuleRef &rule,
    const SymbolRef &form,
    const Definitions &definitions) {

    FormatRule *format_rule;

    if (form == definitions.symbols().All) {
        format_rule = new FormatRule(rule);
    } else {
        format_rule = new FormatRule(rule, form);
    }

    mutable_rules()->format_values.add(FormatRuleRef(format_rule));
}

Definitions::Definitions() : m_symbols(*this), number_form(m_symbols) {
}

SymbolRef Definitions::new_symbol(const char *name, SymbolName symbol_name) {
    assert(m_definitions.find(name) == m_definitions.end());
    const SymbolRef symbol = Pool::Symbol(name, ExtendedType(symbol_name));
    m_definitions[SymbolKey(symbol)] = symbol;
    return symbol;
}

const Symbol *Definitions::system_symbol(const char *name, SymbolName symbol) {
	std::ostringstream fullname;
	fullname << "System`";
	if (strncmp(name, "State", 5) == 0) {
		fullname << "$";
		fullname << name + 5;
	} else if (name[0] == '_') {
		fullname << name + 1;
	} else {
		fullname << name;
	}
	return new_symbol(fullname.str().c_str(), symbol).get();
}

SymbolRef Definitions::lookup(const char *name) {
    const auto it = m_definitions.find(SymbolKey(name));

    if (it == m_definitions.end()) {
        return new_symbol(name);
    } else {
        return it->second;
    }
}

void Definitions::freeze_as_builtin() {
	for (auto i = m_definitions.begin(); i != m_definitions.end(); i++) {
		const MutableSymbolRef &symbol = i->second;
		SymbolRef(symbol)->freeze_as_builtin();
	}
}

void Definitions::reset_user_definitions() {
    auto i = m_definitions.begin();
    while (i != m_definitions.end()) {
        const MutableSymbolRef &symbol = i->second;
        SymbolRef(symbol)->reset_user_definitions();
        i++;
        // i = m_definitions.erase(i);
    }
}
