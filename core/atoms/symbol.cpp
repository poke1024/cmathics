#include "core/types.h"
#include "core/expression/implementation.h"
#include "symbol.h"
#include "core/builtin.h"
#include "core/pattern/matcher.tcc"

thread_local EvaluationContext *EvaluationContext::s_current = nullptr;

void Messages::add(
    const SymbolRef &name,
    const char *tag,
    const char *text,
    Definitions &definitions) {

	m_rules.add(
		new DownRule(
			expression(definitions.symbols().MessageName, name, Pool::String(std::string(tag))),
			Pool::String(std::string(text)),
            definitions));
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
    Definitions &definitions) const {

	SymbolRules *rules = state().mutable_rules();
    rules->messages.ensure([] () {
        return new Messages();
    })->add(SymbolRef(this), tag, text, definitions);
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
	set_attributes(Attributes::None);
    m_own_value.reset();
    m_rules.reset();
    m_copy_on_write = false;
}

void Symbol::reset_user_definitions() const {
	if (m_builtin_state) {
		m_user_state.set_as_copy_of(*m_builtin_state);
	} else {
		m_user_state.clear();
	}
}

BaseExpressionRef Symbol::replace_all(const ArgumentsMap &replacement) const {
	const auto i = replacement.find(this);
	if (i != replacement.end()) {
		return i->second;
	} else {
		return BaseExpressionRef();
	}
}
