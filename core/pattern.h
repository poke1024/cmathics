#ifndef PATTERN_H
#define PATTERN_H

#include <assert.h>

#include "types.h"
#include "definitions.h"

#include <iostream>

class Blank : public Symbol {
public:
    Blank(Definitions *definitions) :
        Symbol(definitions, "System`Blank") {
    }

    virtual match_sizes_t match_num_args_with_head(const Expression *patt) const {
        return std::make_tuple(1, 1);
    }

    virtual Match match_sequence_with_head(
        const Slice &pattern,
        const Slice &sequence) const {

        if (pattern.size() == 1) {
            if (sequence.size() == 1) {
                return Match(true);
            } else {
                return Match(false);
            }
        } else {
            return match_rest(pattern, sequence);
        }
    }
};


class BlankSequence : public Symbol {
public:
    BlankSequence(Definitions *definitions) :
        Symbol(definitions, "System`BlankSequence") {
    }

    virtual match_sizes_t match_num_args_with_head(const Expression *patt) const {
        return std::make_tuple(1, MATCH_MAX);
    }

    virtual Match match_sequence_with_head(
        const Slice &pattern,
        const Slice &sequence) const {

        if (pattern.size() == 1) {
            if (sequence.size() >= 1) {
                return Match(true);
            } else {
                return Match(false);
            }
        } else {
            for (size_t l = sequence.size(); l >= 1; l--) {
                auto match = match_rest(pattern, sequence, l);
                if (match) {
                    return match;
                }
            }

            return Match(false);
        }
    }
};

class BlankNullSequence : public Symbol {
public:
    BlankNullSequence(Definitions *definitions) :
        Symbol(definitions, "System`BlankNullSequence") {
    }

    virtual match_sizes_t match_num_args_with_head(const Expression *patt) const {
        return std::make_tuple(1, MATCH_MAX);
    }
};

class Pattern : public Symbol {
public:
    Pattern(Definitions *definitions) :
        Symbol(definitions, "System`Pattern") {
    }

    virtual Match match_sequence_with_head(
        const Slice &pattern,
        const Slice &sequence) const {

        auto patt = static_cast<const Expression*>(pattern[0]);
        auto item = sequence[0];

        auto var_expr = patt->_leaves[0];
        assert(var_expr->type() == SymbolType);
        // if nullptr -> throw self.error('patvar', expr)
        const Symbol *var = static_cast<const Symbol*>(var_expr);

        const BaseExpression *existing = var->matched_value();
        if (existing) {
            if (existing->same(item)) {
                return match_rest(pattern, sequence);
            } else {
                return Match(false);
            }
        } else if (patt->_leaves[1]->match(item)) {
            Match match;

            var->set_matched_value(item);
            try {
                match = match_rest(pattern, sequence);
            } catch(...) {
                var->clear_matched_value();
                throw;
            }
            var->clear_matched_value();

            if (match) {
                match.add_variable(var, item);
            }

            return match;
        } else {
            return Match(false);
        }
    }

    virtual match_sizes_t match_num_args_with_head(const Expression *patt) const {
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

    virtual match_sizes_t match_num_args_with_head(const Expression *patt) const {
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

    virtual match_sizes_t match_num_args_with_head(const Expression *patt) const {
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
