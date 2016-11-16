#ifndef PATTERN_H
#define PATTERN_H

#include <assert.h>

#include "types.h"
#include "definitions.h"
#include "evaluation.h"
#include "matcher.h"

#include <iostream>

class Blank : public Symbol {
public:
	Blank(Definitions *definitions) :
		Symbol(definitions, "System`Blank", SymbolBlank) {
	}

	virtual OptionalMatchSize match_size_with_head(ExpressionPtr patt) const {
		return MatchSize::exactly(1);
	}
};

class BlankSequence : public Symbol {
public:
    BlankSequence(Definitions *definitions) :
	    Symbol(definitions, "System`BlankSequence", SymbolBlankSequence) {
    }

	virtual OptionalMatchSize match_size_with_head(ExpressionPtr patt) const {
		return MatchSize::at_least(1);
	}
};

class BlankNullSequence : public Symbol {
public:
    BlankNullSequence(Definitions *definitions) :
	    Symbol(definitions, "System`BlankNullSequence", SymbolBlankNullSequence) {
    }

	virtual OptionalMatchSize match_size_with_head(ExpressionPtr patt) const {
		return MatchSize::at_least(0);
	}
};

class Pattern : public Symbol {
public:
    Pattern(Definitions *definitions) :
        Symbol(definitions, "System`Pattern", SymbolPattern) {
    }

    virtual OptionalMatchSize match_size_with_head(ExpressionPtr patt) const {
        if (patt->size() == 2) {
            // Pattern is only valid with two arguments
            return patt->leaf(1)->match_size();
        } else {
            return MatchSize::exactly(1);
        }
    }
};

class Alternatives : public Symbol {
public:
    Alternatives(Definitions *definitions) :
        Symbol(definitions, "System`Alternatives") {
    }

    virtual OptionalMatchSize match_size_with_head(ExpressionPtr patt) const {
	    const size_t n = patt->size();

        if (n == 0) {
            return MatchSize::exactly(1);
        }

        const MatchSize size = patt->leaf(0)->match_size();
        match_size_t min_p = size.min();
	    match_size_t max_p = size.max();

        for (size_t i = 1; i < n; i++) {
	        const MatchSize leaf_size = patt->leaf(i)->match_size();
            max_p = std::max(max_p, leaf_size.max());
            min_p = std::min(min_p, leaf_size.min());
        }

        return MatchSize::between(min_p, max_p);
    }
};

class Repeated : public Symbol {
public:
    Repeated(Definitions *definitions) :
        Symbol(definitions, "System`Repeated") {
    }

    virtual OptionalMatchSize match_size_with_head(ExpressionPtr patt) const {
        switch(patt->size()) {
            case 1:
                return MatchSize::at_least(1);
            case 2:
                // TODO inspect second arg
                return MatchSize::at_least(1);
            default:
                return MatchSize::exactly(1);
        }
    }
};

#endif
