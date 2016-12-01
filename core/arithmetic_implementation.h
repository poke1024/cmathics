#ifndef CMATHICS_ARITHMETIC_IMPL_H_H
#define CMATHICS_ARITHMETIC_IMPL_H_H

#include "arithmetic.h"
#include "integer.h"
#include "real.h"
#include "rational.h"
#include "expression.h"
#include "numeric.h"

template<typename T>
RuleRef NewRule(const SymbolRef &head, const Definitions &definitions) {
	return std::make_shared<T>(head, definitions);
}

template<typename T>
inline BaseExpressionRef add_only_integers(const T &self) {
	// sums an all MachineInteger/BigInteger expression

	Numeric::Z result(0);

	for (auto value : self.template primitives<Numeric::Z>()) {
		result += value;
	}

    return result.to_expression();
}

template<typename T>
inline BaseExpressionRef add_only_machine_reals(const T &self) {
	// sums an all MachineReal expression

	machine_real_t result = 0.;

	for (machine_real_t value : self.template primitives<machine_real_t>()) {
		result += value;
	}

	return Heap::MachineReal(result);
}

template<typename T>
inline BaseExpressionRef add_machine_inexact(const T &self) {
	// create an array to store all the symbolic arguments which can't be evaluated.

	std::vector<BaseExpressionRef> symbolics;
	symbolics.reserve(self.size());

	machine_real_t sum = 0.0;
	for (const BaseExpressionRef leaf : self.leaves()) {
		const BaseExpression *leaf_ptr = leaf.get();

		const auto type = leaf_ptr->type();
		switch(type) {
			case MachineIntegerType:
				sum += machine_real_t(static_cast<const MachineInteger*>(leaf_ptr)->value);
				break;
			case BigIntegerType:
				sum += static_cast<const BigInteger*>(leaf_ptr)->value.get_d();
				break;
			case MachineRealType:
				sum += static_cast<const MachineReal*>(leaf_ptr)->value;
				break;
			case BigRealType:
				sum += static_cast<const BigReal*>(leaf_ptr)->as_double();
				break;
			case BigRationalType:
				sum += static_cast<const BigRational*>(leaf_ptr)->value.get_d();
				break;
			case ComplexType:
				assert(false);
				break;
			case ExpressionType:
			case SymbolType:
			case StringType:
				symbolics.push_back(leaf);
				break;

			default:
				throw std::runtime_error("unsupported types"); // FIXME
		}
	}

	// at least one non-symbolic
	assert(symbolics.size() != self.size());

	BaseExpressionRef result;

	if (symbolics.size() == self.size() - 1) {
		// one non-symbolic: nothing to do
		// result = NULL;
	} else if (!symbolics.empty()) {
		// at least one symbolic
		symbolics.push_back(from_primitive(sum));
		result = expression(self._head, std::move(symbolics));
	} else {
		// no symbolics
		result = from_primitive(sum);
	}

	return result;
}

template<typename T>
BaseExpressionRef ArithmeticOperationsImplementation<T>::Plus() const {
	const T &expr = this->expr();

	// we guarantee that expr.size() >= 3 through the given match_size() in the corresponding
	// Rule.

	constexpr TypeMask int_mask = MakeTypeMask(BigIntegerType) | MakeTypeMask(MachineIntegerType);

	// bit field to determine which types are present
	const TypeMask types_seen = expr.exact_type_mask();

	// expression is all MachineReals
	if (types_seen == MakeTypeMask(MachineRealType)) {
		return add_only_machine_reals(expr);
	}

	// expression is all Integers
	if ((types_seen & int_mask) == types_seen) {
		return add_only_integers(expr);
	}

	// expression contains a Real
	if (types_seen & MakeTypeMask(MachineRealType)) {
		return add_machine_inexact(expr);
	}

	// expression contains an Integer
	/*if (types_seen & int_mask) {
		auto integer_result = expr->operations().add_integers();
		// FIXME return Plus[symbolics__, integer_result]
		return integer_result;
	}*/

	// TODO rational and complex

	// expression is symbolic
	return BaseExpressionRef();
}

#endif //CMATHICS_ARITHMETIC_IMPL_H_H
