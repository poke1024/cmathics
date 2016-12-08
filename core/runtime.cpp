#include "runtime.h"

Runtime::Runtime() : _parser(_definitions) {

    const SymbolRef General = _definitions.symbols().General;

    General->add_message("normal", "Nonatomic expression expected.", _definitions);
    General->add_message("iterb", "Iterator does not have appropriate bounds.", _definitions);
	General->add_message("level", "Level specification `1` is not of the form n, {n}, or {m, n}.", _definitions);
    General->add_message("optx", "Unknown option `1` in `2`.", _definitions);
}

void Runtime::add(
    const char *name,
    Attributes attributes,
    const std::initializer_list<NewRuleRef> &rules) {

    const std::string full_down = std::string("System`") + name;
    const SymbolRef symbol = _definitions.lookup(full_down.c_str());
    symbol->set_attributes(attributes);
    for (const NewRuleRef &new_rule : rules) {
        symbol->add_rule(new_rule(symbol, _definitions));
    }
}
