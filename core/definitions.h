#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "symbol.h"
#include "expression.h"
#include "heap.h"

class Definitions;

class Symbols {
public:
	inline Symbols() {
	}

	inline Symbols(const Symbols &symbols) {
		std::memcpy(this, &symbols, sizeof(symbols));
	}

	Symbol *List;
	Symbol *Sequence;

	Symbol *False;
	Symbol *True;
	Symbol *Null;

	Symbol *Blank;
	Symbol *BlankSequence;
	Symbol *BlankNullSequence;
	Symbol *Pattern;
	Symbol *Alternatives;
	Symbol *Repeated;

	Symbol *Slot;
	Symbol *SlotSequence;
	Symbol *Function;

	inline Symbol *Boolean(bool x) const {
		return x ? True : False;
	}
};

class Definitions {
private:
    std::map<std::string,SymbolRef> _definitions;
	Symbols _symbols;

    Symbol *add_internal_symbol(const SymbolRef &symbol);
	SymbolRef new_symbol(const char *name, ExtendedType symbol = SymbolExtendedType);

public:
    Definitions();

    SymbolRef lookup(const char *name);

	inline const Symbols &symbols() const {
		return _symbols;
	}
};

#endif
