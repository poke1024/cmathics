#include "core/types.h"
#include "core/expression/implementation.h"
#include "symbol.h"
#include "core/builtin.h"
#include "core/matcher/matcher.tcc"

void Messages::add(
    const SymbolRef &name,
    const char *tag,
    const char *text,
    const Evaluation &evaluation) {

	m_rules.add(
		DownRule::construct(
			expression(evaluation.MessageName, name, String::construct(std::string(tag))),
			String::construct(std::string(text)),
			evaluation),
		evaluation);
}

StringRef Messages::lookup(
    const Expression *message,
    const Evaluation &evaluation) const {

	const optional<BaseExpressionRef> result = m_rules.apply(
		message, evaluation);

    if (result && *result && (*result)->is_string()) {
        return (*result)->as_string();
    }

    return StringRef();
}

std::string Symbol::debugform() const {
	return std::string(name());
}

BaseExpressionPtr Symbol::head(const Symbols &symbols) const {
    return symbols._Symbol;
}

void Symbol::add_message(
    const char *tag,
    const char *text,
    const Evaluation &evaluation) const {

	SymbolRules *rules = mutable_state().mutable_rules();
    rules->messages.ensure([] () {
        return Messages::construct();
    })->add(SymbolRef(this), tag, text, evaluation);
}

StringRef Symbol::lookup_message(
    const Expression *message,
    const Evaluation &evaluation) const {

	const SymbolRules *rules = state().rules();
    if (rules && rules->messages) {
        return rules->messages->lookup(message, evaluation);
    } else {
        return StringRef();
    }
}

SymbolicFormRef Symbol::instantiate_symbolic_form(const Evaluation &evaluation) const {
    switch (symbol()) {
	    case S::I:
            return SymbolicForm::construct(SymEngine::I);

	    case S::Pi:
            return SymbolicForm::construct(SymEngine::pi);

	    case S::E:
            return SymbolicForm::construct(SymEngine::E);

	    case S::EulerGamma:
            return SymbolicForm::construct(SymEngine::EulerGamma);

        default: {
	        std::string name;

#if DEBUG_SYMBOLIC
			name = this->name();
#else
            // quite a hack currently. we store the pointer to our symbol to avoid having
            // to make a full name lookup. as symbolic evaluation always happens in the
            // context of an existing, referenced Expression, this should be fine.
            const Symbol * const addr = this;
            name.append(reinterpret_cast<const char*>(&addr), sizeof(addr) / sizeof(char));
#endif
	        return SymbolicForm::construct(SymEngine::symbol(name));
        }
    }
}

void SymbolState::clear() {
	clear_attributes();
    m_own_value.reset();
    m_rules.reset();
    m_copy_on_write = false;
}

void Symbol::reset_user_definitions() const {
	if (m_builtin_state) {
		m_state.modify().set_as_copy_of(*m_builtin_state);
	} else {
		m_state.modify().clear();
	}
}

BaseExpressionRef Symbol::replace_all(const ArgumentsMap &replacement, const Evaluation &evaluation) const {
	const auto i = replacement.find(this);
	if (i != replacement.end()) {
		return i->second;
	} else {
		return BaseExpressionRef();
	}
}

BaseExpressionRef Symbol::make_boxes(
    BaseExpressionPtr form,
    const Evaluation &evaluation) const {

    return String::construct(short_name());
}

MatchSize Symbol::string_match_size() const {
    switch (symbol()) {
        case S::DigitCharacter:
        case S::WhitespaceCharacter:
        case S::WordCharacter:
        case S::LetterCharacter:
        case S::HexidecimalCharacter:
            return MatchSize::exactly(1);

        case S::Whitespace:
            return MatchSize::at_least(0);

        default:
            return MatchSize::exactly(0);
    }
}
