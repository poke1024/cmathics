#include "symbol.h"

bool Symbol::instantiate_symbolic_form() const {
    switch (extended_type()) {
        case SymbolI:
            set_symbolic_form(SymEngine::I);
            return true;

        case SymbolPi:
            set_symbolic_form(SymEngine::pi);
            return true;

        case SymbolE:
            set_symbolic_form(SymEngine::E);
            return true;

        case SymbolEulerGamma:
            set_symbolic_form(SymEngine::EulerGamma);
            return true;

        default: {
            // quite a hack currently. we store the pointer to our symbol to avoid having
            // to make a full name lookup. as symbolic evaluation always happens in the
            // context of an existing, referenced Expression, this should be fine.
            const Symbol * const addr = this;
            std::string name;
            name.append(reinterpret_cast<const char*>(&addr), sizeof(addr) / sizeof(char));
            set_symbolic_form(SymEngine::symbol(name));
            return true;
        }
    }
}