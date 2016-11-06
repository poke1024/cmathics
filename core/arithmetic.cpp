#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <vector>

#include "types.h"
#include "pattern.h"
#include "integer.h"
#include "arithmetic.h"
#include "real.h"
#include "rational.h"


// sums all integers in an expression
BaseExpressionRef add_Integers(const ExpressionRef &expr) {
	mpz_class result(0);

    // XXX right now the entire computation is done with GMP. This is slower
    // than using machine precision arithmetic but simpler because we don't
    // need to handle overflow. Could be optimised.

    for (auto leaf : expr->_leaves) {
        switch (leaf->type()) {
	        case MachineIntegerType: {
		        auto value = std::static_pointer_cast<const MachineInteger>(leaf)->value;
		        if (value >= 0) {
			        result += static_cast<unsigned long>(value);
		        } else {
			        result -= static_cast<unsigned long>(-value);
		        }
		        break;
	        }
	        case BigIntegerType:
		        result += std::static_pointer_cast<const BigInteger>(leaf)->value;
		        break;
	        default:
		        assert(false);
        }
    }

	return integer(result);
}


BaseExpressionRef add_MachineInexact(const ExpressionRef &expr) {
    // create an array to store all the symbolic arguments which can't be evaluated.
    auto symbolics = std::vector<BaseExpressionRef>();
    symbolics.reserve(expr->_leaves.size());

    double sum = 0.0;
    for (auto leaf : expr->_leaves) {
        auto type = leaf->type();
        switch(type) {
            case MachineIntegerType:
                sum += (double) std::static_pointer_cast<const MachineInteger>(leaf)->value;
                break;
            case BigIntegerType:
                sum += std::static_pointer_cast<const BigInteger>(leaf)->value.get_d();
                break;
            case MachineRealType:
                sum += std::static_pointer_cast<const MachineReal>(leaf)->value;
                break;
            case BigRealType:
                sum += std::static_pointer_cast<const BigReal>(leaf)->_value.toDouble(MPFR_RNDN);
                break;
            case RationalType:
                sum += std::static_pointer_cast<const Rational>(leaf)->value.get_d();
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

    BaseExpressionRef result;

    if (symbolics.size() == expr->_leaves.size() - 1) {
        // one non-symbolic: nothing to do
        // result = NULL;
    } else if (!symbolics.empty()) {
        // at least one symbolic
        symbolics.push_back(real(sum));
        result = expression(expr->_head, symbolics);
    } else {
        // no symbolics
        result = real(sum);
    }

    return result;
}


BaseExpressionRef Plus(const ExpressionRef &expr, const Evaluation &evaluation) {
    switch (expr->_leaves.size()) {
        case 0:
            // Plus[] -> 0
            return integer(0LL);

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
        auto integer_result = add_Integers(expr);
        // FIXME return Plus[symbolics__, integer_result]
        return integer_result;
    }

    // TODO rational and complex

    // expression is symbolic
    return BaseExpressionRef();
}
