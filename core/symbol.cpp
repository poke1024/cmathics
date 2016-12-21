#include "types.h"
#include "expression.h"
#include "symbol.h"
#include "builtin.h"

void Messages::add(
    const SymbolRef &name,
    const char *tag,
    const char *text,
    const Definitions &definitions) {

	m_rules.add(
		std::make_shared<RewriteRule>(
			expression(definitions.symbols().MessageName, name, Heap::String(std::string(tag))),
				Heap::String(std::string(text))));
}

StringRef Messages::lookup(
    const Expression *message,
    const Evaluation &evaluation) const {

	const BaseExpressionRef result = m_rules.try_and_apply(
		message->slice_code(), message, evaluation);

    if (result && result->type() == StringType) {
        return boost::static_pointer_cast<const String>(result);
    }

    return StringRef();
}

BaseExpressionPtr Symbol::head(const Evaluation &evaluation) const {
    return evaluation._Symbol;
}

void Symbol::add_message(
    const char *tag,
    const char *text,
    const Definitions &definitions) {

	SymbolRules *rules = create_rules();
    if (!rules->messages) {
	    rules->messages = std::make_shared<Messages>();
    }
	rules->messages->add(SymbolRef(this), tag, text, definitions);
}

StringRef Symbol::lookup_message(
    const Expression *message,
    const Evaluation &evaluation) const {

	const SymbolRulesRef &rules = _rules;
    if (rules && rules->messages) {
        return rules->messages->lookup(message, evaluation);
    } else {
        return StringRef();
    }
}

SymbolicFormRef Symbol::instantiate_symbolic_form() const {
    switch (extended_type()) {
        case SymbolI:
            return Heap::SymbolicForm(SymEngine::I);

        case SymbolPi:
            return Heap::SymbolicForm(SymEngine::pi);

        case SymbolE:
            return Heap::SymbolicForm(SymEngine::E);

        case SymbolEulerGamma:
            return Heap::SymbolicForm(SymEngine::EulerGamma);

        default: {
            // quite a hack currently. we store the pointer to our symbol to avoid having
            // to make a full name lookup. as symbolic evaluation always happens in the
            // context of an existing, referenced Expression, this should be fine.
            const Symbol * const addr = this;
            std::string name;
            name.append(reinterpret_cast<const char*>(&addr), sizeof(addr) / sizeof(char));
            return Heap::SymbolicForm(SymEngine::symbol(name));
        }
    }
}