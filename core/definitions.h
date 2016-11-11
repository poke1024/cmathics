#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "symbol.h"
#include "expression.h"
#include "heap.h"

class Definitions {
private:
    Heap _heap;

    std::map<std::string,SymbolRef> _definitions;
    ExpressionRef _empty_list;

	SymbolRef _list;
    SymbolRef _sequence;
    SymbolRef _false;
    SymbolRef _true;

    void add_internal_symbol(const SymbolRef &symbol);

public:
    Definitions();

    SymbolRef new_symbol(const char *name, Type symbol = SymbolType);

    SymbolRef lookup(const char *name);

    inline const ExpressionRef &empty_list() const {
        return _empty_list;
    }

    inline const SymbolRef &List() const {
        return _list;
    }

    inline const SymbolRef &Sequence() const {
        return _sequence;
    }

    inline const SymbolRef &False() const {
        return _false;
    }

    inline const SymbolRef &True() const {
        return _true;
    }
};

#endif
