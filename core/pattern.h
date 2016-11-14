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
};

class BlankSequence : public Symbol {
public:
    BlankSequence(Definitions *definitions) :
	    Symbol(definitions, "System`BlankSequence", SymbolBlankSequence) {
    }
};

class BlankNullSequence : public Symbol {
public:
    BlankNullSequence(Definitions *definitions) :
	    Symbol(definitions, "System`BlankNullSequence", SymbolBlankNullSequence) {
    }
};

class Pattern : public Symbol {
public:
    Pattern(Definitions *definitions) :
        Symbol(definitions, "System`Pattern", SymbolPattern) {
    }

    virtual match_sizes_t match_num_args_with_head(DynamicExpressionPtr patt) const {
        if (patt->_leaves.size() == 2) {
            // Pattern is only valid with two arguments
            return patt->_leaves[1]->match_num_args();
        } else {
            return std::make_tuple(1, 1);
        }
    }
};

class Alternatives : public Symbol {
public:
    Alternatives(Definitions *definitions) :
        Symbol(definitions, "System`Alternatives") {
    }

    virtual match_sizes_t match_num_args_with_head(DynamicExpressionPtr patt) const {
        auto leaves = patt->_leaves;

        if (leaves.size() == 0) {
            return std::make_tuple(1, 1);
        }

        size_t min_p, max_p;
        std::tie(min_p, max_p) = leaves[0]->match_num_args();

        for (auto i = std::begin(leaves) + 1; i != std::end(leaves); i++) {
            size_t min_leaf, max_leaf;
            std::tie(min_leaf, max_leaf) = (*i)->match_num_args();
            max_p = std::max(max_p, max_leaf);
            min_p = std::min(min_p, min_leaf);
        }

        return std::make_tuple(min_p, max_p);
    }
};

class Repeated : public Symbol {
public:
    Repeated(Definitions *definitions) :
        Symbol(definitions, "System`Repeated") {
    }

    virtual match_sizes_t match_num_args_with_head(DynamicExpressionPtr patt) const {
        switch(patt->_leaves.size()) {
            case 1:
                return std::make_tuple(1, MATCH_MAX);
            case 2:
                // TODO inspect second arg
                return std::make_tuple(1, MATCH_MAX);
            default:
                return std::make_tuple(1, 1);
        }
    }
};

#endif
