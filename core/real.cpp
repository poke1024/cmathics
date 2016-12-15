#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include "types.h"
#include "real.h"
#include "expression.h"

BaseExpressionPtr MachineReal::head(const Evaluation &evaluation) const {
    return evaluation.Real;
}

BaseExpressionPtr BigReal::head(const Evaluation &evaluation) const {
    return evaluation.Real;
}

// determine precision of an expression
// returns 0 if the precision is infinite
// returns 1 if the precision is machine-valued
// returns 2 if the precision is big in which case result is also set

std::pair<int32_t,double> precision_of(const BaseExpressionRef &expr) {
    return std::pair<int32_t,double>(0,  0.0); // FIXME

    /*switch (expr->type()) {
        case MachineRealType:
            return std::pair<int32_t,double>(1, 0.0);
        case BigRealType: {
            auto big_expr = (const BigReal *) expr.get();
            return std::pair<int32_t, double>(2, big_expr->_prec);
        }
        case ExpressionType: {
            bool first_big = false;
            const Expression *expr_expr = static_cast<const Expression*>(expr.get());
            double result = 0.0;
            for (auto leaf : *expr_expr) {
                auto r = precision_of(leaf);
                auto ret_code = r.first;
                auto leaf_result = r.second;

                if (ret_code == 1) {
                    return std::pair<int32_t, double>(1, result);
                } else if (ret_code == 2) {
                    if (first_big) {
                        result = leaf_result;
                        first_big = false;
                    } else {
                        if (leaf_result < result) {
                            result = leaf_result;
                        }
                    }
                } else {
                    assert(ret_code == 0);
                }
            }
            if (first_big) {
                return std::pair<int32_t, double>(0, result);
            } else {
                return std::pair<int32_t, double>(2, result);
            }
        }
        case ComplexType:
            // TODO
        default:
            return std::pair<int32_t,double>(0,  0.0);
    }*/
}
