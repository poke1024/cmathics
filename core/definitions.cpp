#include "types.h"
#include "core/expression/implementation.h"
#include "evaluation.h"
#include "core/pattern/arguments.h"
#include "builtin.h"
#include "core/matcher/matcher.tcc"
#include "arithmetic/binary.h"
#include "arithmetic/compare.tcc"

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
	} else if (pattern->as_expression()->head()->symbol() == S::Condition && pattern->as_expression()->size() == 2) {
		const BaseExpressionRef expr = pattern->as_expression()->n_leaves<2>()[0];
		return get_definitions_pos(expr.get(), symbol);
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
    BaseExpression(symbol), m_state(this) {

	const size_t n = snprintf(
		_short_name, sizeof(_short_name), "%s", name);
	if (n >= sizeof(_short_name)) {
		_name = new char[strlen(name) + 1];
		strcpy(_name, name);
	} else {
		_name = _short_name;
	}

	mutable_state().clear_attributes();
}

Symbol::~Symbol() {
	if (_name != _short_name) {
		delete[] _name;
	}
}

BaseExpressionRef Symbol::replace_all(const MatchRef &match, const Evaluation &evaluation) const {
	const UnsafeBaseExpressionRef *value = match->get_matched_value(this);
	if (value) {
		return *value;
	} else {
		return BaseExpressionRef();
	}
}

void SymbolRules::set_attributes(
    Attributes attributes,
    const Evaluation &evaluation) {

    down_rules.set_governing_attributes(attributes, evaluation);
}

void SymbolState::clear_attributes() {
	m_attributes = Attributes::None;
	m_dispatch = EvaluateDispatch::pick(Attributes::None);
}

void SymbolState::clear_attributes(const Evaluation &evaluation) {
	clear_attributes();
	evaluation.definitions.update_version();
    if (m_rules) {
        m_rules->set_attributes(m_attributes, evaluation);
    }
}

void SymbolState::add_attributes(Attributes attributes, const Evaluation &evaluation) {
	m_attributes = m_attributes + attributes;
    m_dispatch = EvaluateDispatch::pick(attributes);
    evaluation.definitions.update_version();
    if (m_rules) {
        m_rules->set_attributes(m_attributes, evaluation);
    }
}

void SymbolState::remove_attributes(Attributes attributes, const Evaluation &evaluation) {
	m_attributes = m_attributes - attributes;
	m_dispatch = EvaluateDispatch::pick(attributes);
    evaluation.definitions.update_version();
    if (m_rules) {
        m_rules->set_attributes(m_attributes, evaluation);
    }
}

void SymbolState::add_rule(
	BaseExpressionPtr lhs,
	BaseExpressionPtr rhs,
	const Evaluation &evaluation) {

	switch (get_definitions_pos(lhs, m_symbol)) {
		case DefinitionsPos::None:
			break;
		case DefinitionsPos::Own:
			m_own_value = rhs;
			break;
        case DefinitionsPos::Up:
            add_up_rule(UpRule::construct(lhs, rhs, evaluation), evaluation);
            break;
		case DefinitionsPos::Down:
			add_down_rule(DownRule::construct(lhs, rhs, evaluation), evaluation);
			break;
		case DefinitionsPos::Sub:
			add_sub_rule(SubRule::construct(lhs, rhs, evaluation), evaluation);
			break;
	}

	evaluation.definitions.update_version();
}

void SymbolState::add_rule(
	const RuleRef &rule,
	const Evaluation &evaluation) {

	switch (get_definitions_pos(rule->pattern.get(), m_symbol)) {
		case DefinitionsPos::None:
			break;
		case DefinitionsPos::Own:
			m_own_value = rule->rhs();
			break;
        case DefinitionsPos::Up:
            add_up_rule(rule, evaluation);
            break;
		case DefinitionsPos::Down:
			add_down_rule(rule, evaluation);
			break;
		case DefinitionsPos::Sub:
			add_sub_rule(rule, evaluation);
			break;
	}

	evaluation.definitions.update_version();
}

void SymbolState::add_format(
    const RuleRef &rule,
    const SymbolRef &form,
    const Evaluation &evaluation) {

    UnsafeFormatRuleRef format_rule;

    if (form == evaluation.All) {
        format_rule = FormatRule::construct(rule);
    } else {
        format_rule = FormatRule::construct(rule, form);
    }

    mutable_rules()->format_values.add(format_rule, evaluation);

	evaluation.definitions.update_version();
}

bool SymbolState::has_format(
	const BaseExpressionRef &lhs,
	const Evaluation &evaluation) const {

	return rules() && rules()->format_values.has_rule_with_pattern(lhs, evaluation);
}

Definitions::Definitions() :
    m_version(Version::construct()), // needed first
    m_symbols(*this),
    number_form(m_symbols),
    zero(MachineInteger::construct(0)),
    one(MachineInteger::construct(1)),
    minus_one(MachineInteger::construct(-1)),
    no_symbolic_form(SymbolicForm::construct(SymEngineRef())),
    default_match(Match::construct()),
    empty_list(expression(m_symbols.List)),
    empty_sequence(expression(m_symbols.Sequence)),
    order(new BinaryOperator<struct order>(*this)) {
}

Definitions::~Definitions() {
}

SymbolRef Definitions::new_symbol(const char *name, SymbolName symbol_name) {
    assert(m_definitions.find(name) == m_definitions.end());
    const SymbolRef symbol = Symbol::construct(name, ExtendedType(symbol_name));
    m_definitions[SymbolKey(symbol)] = symbol;
	update_master_version();
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
	update_master_version();
}

void Definitions::update_master_version() {
	m_version.set_master(Version::construct());
}

VersionRef Definitions::master_version() const {
	return m_version.get_master();
}

