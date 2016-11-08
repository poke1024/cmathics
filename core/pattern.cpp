#include <stdbool.h>
#include <string.h>
#include <limits.h>

#include "pattern.h"
#include "expression.h"
#include "definitions.h"
#include "integer.h"
#include "string.h"
#include "real.h"

void MatchContext::add_matched_variable(const Symbol *variable) {
    variable->link_variable(_matched_variables_head);
    _matched_variables_head = variable;
}

const Symbol *blank_head(RefsExpressionPtr patt) {
    switch (patt->_leaves.size()) {
        case 0:
            return nullptr;

        case 1: {
            auto symbol = patt->leaf_ptr(0);
            if (symbol->type() == SymbolType) {
                return static_cast<const Symbol *>(symbol);
            }
        }
        // fallthrough

        default:
            return nullptr;
    }
}
