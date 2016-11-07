#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "symbol.h"
#include "expression.h"

class Definitions {
private:
    std::map<std::string,SymbolRef> _definitions;
    std::shared_ptr<RefsExpression> _empty_list;

    SymbolRef _sequence;
	SymbolRef _list;

    void add_internal_symbol(const SymbolRef &symbol);

public:
    Definitions();

    SymbolRef new_symbol(const char *name);

    SymbolRef lookup(const char *name);

    inline RefsExpression *empty_list() const {
        return _empty_list.get();
    }

    inline const SymbolRef &list() const {
        return _list;
    }

    inline const SymbolRef &sequence() const {
        return _sequence;
    }
};

#endif
