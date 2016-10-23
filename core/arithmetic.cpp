#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <vector>

#include "types.h"
#include "integer.h"
#include "arithmetic.h"
#include "real.h"
#include "rational.h"


// sums all integers in an expression
Integer* add_Integers(Expression* expr) {
    uint32_t i;
    int64_t machine_leaf;
    mpz_t result;
    Integer* return_value;

    // XXX right now the entire computation is done with GMP. This is slower
    // than using machine precision arithmetic but simpler because we don't
    // need to handle overflow. Could be optimised.

    mpz_init(result);

    for (auto leaf : expr->_leaves) {
        auto type = leaf->type();
        if (type == MachineIntegerType) {
            machine_leaf = ((MachineInteger*)leaf)->value;
            if (machine_leaf >= 0) {
                mpz_add_ui(result, result, (uint64_t) machine_leaf);
            } else {
                mpz_add_ui(result, result, (uint64_t) -machine_leaf);
            }
        } else if (type == BigIntegerType) {
            mpz_add(result, result, ((BigInteger*)leaf)->value);
        }
    }

    return_value = Integer_from_mpz(result);
    mpz_clear(result);
    return return_value;
}


BaseExpression* add_MachineInexact(Expression* expr) {
    BaseExpression* result;

    // create an array to store all the symbolic arguments which can't be evaluated.
    auto symbolics = std::vector<BaseExpressionPtr>();
    symbolics.reserve(expr->_leaves.size());

    double sum = 0.0;
    for (auto leaf : expr->_leaves) {
        auto type = leaf->type();
        switch(type) {
            case MachineIntegerType:
                sum += (double) (((MachineInteger*) leaf)->value);
                break;
            case BigIntegerType:
                sum += mpz_get_d(((BigInteger*) leaf)->value);
                break;
            case MachineRealType:
                sum += ((MachineReal*) leaf)->value;
                break;
            case BigRealType:
                sum += mpfr_get_d(((BigReal*) leaf)->value, MPFR_RNDN);
                break;
            case RationalType:
                sum += mpq_get_d(((Rational*) leaf)->value);
                break;
            case ComplexType:
                assert(false);
                break;
            case ExpressionType:
            case SymbolType:
            case StringType:
            case RawExpressionType:
                symbolics.push_back(leaf);
                break;
        }
    }

    // at least one non-symbolic
    assert(symbolics.size() != expr->_leaves.size());

    if (symbolics.size() == expr->_leaves.size() - 1) {
        // one non-symbolic: nothing to do
         result = NULL;
    } else if (!symbolics.empty()) {
        // at least one symbolic
        symbolics.push_back(new MachineReal(sum));
        result = new Expression(expr->_head, symbolics);
    } else {
        // no symbolics
        result = new MachineReal(sum);
    }

    return result;
}


BaseExpressionPtr _Plus(Expression* expr) {
    MachineInteger* machine_result;
    Integer* integer_result;

    switch (expr->_leaves.size()) {
        case 0:
            // Plus[] -> 0
            return new MachineInteger(0);

        case 1:
            // Plus[a_] -> a
            return expr->_leaves[0];
    }

    // bit field to determine which types are present
    uint16_t types_seen = 0;
    for (auto leaf : expr->_leaves) {
        types_seen |= 1 << leaf->type();
    }

    // expression contains a Real
    if (types_seen & (1 << MachineRealType)) {
        return add_MachineInexact(expr);
    }

    constexpr uint16_t int_mask = (1 << BigIntegerType) | (1 << MachineIntegerType);

    // expression is all Integers
    if ((types_seen & int_mask) == types_seen) {
        return add_Integers(expr);
    }

    // expression contains an Integer
    if (types_seen & int_mask) {
        integer_result = add_Integers(expr);
        // FIXME return Plus[symbolics__, integer_result]
        return integer_result;
    }

    // TODO rational and complex

    // expression is symbolic
    return nullptr;
}
