#include <assert.h>
#include <stdlib.h>
#include "misc.h"

int64_t Expression_height(BaseExpressionPtr expression) {
    /*int64_t result;
    uint32_t leaf_height;
    int64_t i;
    Expression* nexpression;

    assert(expression != NULL);
    switch (expression->type()) {
        case ExpressionType:
            nexpression = (Expression*) expression;
            result = Expression_height(nexpression->_head.get());
            for (auto leaf : nexpression->_leaves) {
                leaf_height = Expression_height(leaf.get());
                if (leaf_height > result) {
                    result = leaf_height;
                }
            }
            result++; // for this level
            break;
        default:
            result = 1;
    }
    return result;*/
    return 0;
}
