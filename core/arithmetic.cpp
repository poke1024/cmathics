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

	return from_value(result);
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
        symbolics.push_back(from_value(sum));
        result = expression(expr->_head, symbolics);
    } else {
        // no symbolics
        result = from_value(sum);
    }

    return result;
}


BaseExpressionRef Plus(const ExpressionRef &expr, const Evaluation &evaluation) {
    switch (expr->_leaves.size()) {
        case 0:
            // Plus[] -> 0
            return from_value(0LL);

        case 1:
            // Plus[a_] -> a
            return expr->_leaves[0];
    }

    // bit field to determine which types are present
	TypeMask types_seen = 0;
    for (auto leaf : expr->_leaves) {
        types_seen |= 1 << leaf->type();
    }

    // expression contains a Real
    if (types_seen & (1 << MachineRealType)) {
        return add_MachineInexact(expr);
    }

    constexpr TypeMask int_mask = (1 << BigIntegerType) | (1 << MachineIntegerType);

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

template<typename F>
BaseExpressionRef compute(TypeMask mask, const F &f) {
	// expression contains a Real
	if (mask & (1 << MachineRealType)) {
		return f.template compute<double>();
	}

	constexpr TypeMask machine_int_mask = (1 << MachineIntegerType);
	// expression is all MachineIntegers
	if ((mask & machine_int_mask) == mask) {
		return f.template compute<int64_t>();
	}

	constexpr TypeMask int_mask = (1 << BigIntegerType) | (1 << MachineIntegerType);
	// expression is all Integers
	if ((mask & int_mask) == mask) {
		return f.template compute<mpz_class>();
	}

	constexpr TypeMask rational_mask = (1 << RationalType);
	// expression is all Rationals
	if ((mask & rational_mask) == mask) {
		return f.template compute<mpq_class>();
	}

	// f.compute<mpfr::mpreal>()

	assert(false);
}

class RangeComputation {
private:
	const BaseExpressionRef &_imin;
	const BaseExpressionRef &_imax;
	const BaseExpressionRef &_di;
	const Evaluation &_evaluation;

public:
	inline RangeComputation(
		const BaseExpressionRef &imin,
		const BaseExpressionRef &imax,
		const BaseExpressionRef &di,
		const Evaluation &evaluation) : _imin(imin), _imax(imax), _di(di), _evaluation(evaluation) {
	}

	template<typename T>
	BaseExpressionRef compute() const {
		const T imin = to_value<T>(_imin);
		const T imax = to_value<T>(_imax);
		const T di = to_value<T>(_di);

		std::vector<BaseExpressionRef> leaves;
		for (T x = imin; x <= imax; x += di) {
			leaves.push_back(from_value(x));
		}

		return expression(_evaluation.definitions.list(), leaves);
	}
};

BaseExpressionRef Range(
	const BaseExpressionRef &imin,
    const BaseExpressionRef &imax,
    const BaseExpressionRef &di,
	const Evaluation &evaluation) {
	return compute(
		imin->type_mask() | imax->type_mask() | di->type_mask(),
		RangeComputation(imin, imax, di, evaluation));
}
