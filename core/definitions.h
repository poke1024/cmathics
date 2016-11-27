#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "symbol.h"
#include "expression.h"
#include "heap.h"
#include <unordered_map>

class Definitions;

class Symbols {
private:
	struct {
	} _dummy;

public:
	Symbols(Definitions &definitions);

	inline Symbols(const Symbols &symbols) {
		std::memcpy(this, &symbols, sizeof(symbols));
	}

	#define SYMBOL(SYMBOLNAME) Symbol *SYMBOLNAME;
	#include "system_symbols.h"
	#undef SYMBOL

	inline Symbol *Boolean(bool x) const {
		return x ? True : False;
	}
};

class Definitions {
private:
    std::unordered_map<SymbolKey,SymbolRef> _definitions;
	const Symbols _symbols;

protected:
	friend class Symbols;

	SymbolRef new_symbol(const char *name, ExtendedType symbol = SymbolExtendedType);

	Symbol *system_symbol(const char *name, ExtendedType type);

public:
    Definitions();

    SymbolRef lookup(const char *name);

	inline const Symbols &symbols() const {
		return _symbols;
	}
};

#endif
