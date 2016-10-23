#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include "types.h"
#include "real.h"
#include "expression.h"


const double LOG_2_10 = 3.321928094887362;  // log2(10.0);

BigReal::BigReal(const double new_prec, const double new_value) {
    mpfr_prec_t bits_prec;
    bits_prec = (mpfr_prec_t) ceil(LOG_2_10 * prec);
    // p->base.ref = 0;
    prec = new_prec;
    mpfr_init2(value, bits_prec);
    mpfr_set_d(value, new_value, MPFR_RNDN);
}

// determine precision of an expression
// returns 0 if the precision is infinite
// returns 1 if the precision is machine-valued
// returns 2 if the precision is big in which case result is also set

int32_t precision_of(BaseExpressionPtr expr, double &result) {
    uint32_t i;
    int32_t ret_code;
    double leaf_result;
    bool first_big;
    Expression* expr_expr;
    BigReal* big_expr;

    switch (expr->type()) {
        case MachineRealType:
            result = 0.0;
            return 1;
        case BigRealType:
            big_expr = (BigReal*) expr;
            result = big_expr->prec;
            return 2;
        case ExpressionType:
            first_big = false;
            expr_expr = (Expression*) expr;
            for (auto leaf : expr_expr->_leaves) {
                ret_code = precision_of(leaf, leaf_result);

                if (ret_code == 1) {
                    return 1;
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
                return 0;
            } else {
                return 2;
            }
        case ComplexType:
            // TODO
        default:
            return 0;
    }
}
