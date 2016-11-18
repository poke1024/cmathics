#ifndef CMATHICS_ARITHMETIC_IMPL_H_H
#define CMATHICS_ARITHMETIC_IMPL_H_H

#include "arithmetic.h"
#include "integer.h"
#include "real.h"
#include "rational.h"
#include "expression.h"

template<typename T>
BaseExpressionRef ArithmeticOperationsImplementation<T>::plus() const {
	const T &expr = this->expr();

	switch (expr.size()) {
		case 0:
			// Plus[] -> 0
			return from_primitive(0LL);

		case 1:
			// Plus[a_] -> a
			return expr.leaf(0);
	}

	constexpr TypeMask int_mask = MakeTypeMask(BigIntegerType) | MakeTypeMask(MachineIntegerType);

	// bit field to determine which types are present
	const TypeMask types_seen = expr.type_mask();

	// expression is all MachineReals
	if (types_seen == MakeTypeMask(MachineRealType)) {
		return expr.add_only_machine_reals();
	}

	// expression is all Integers
	if ((types_seen & int_mask) == types_seen) {
		return expr.add_only_integers();
	}

	// expression contains a Real
	if (types_seen & MakeTypeMask(MachineRealType)) {
		return expr.add_machine_inexact();
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

template<typename T>
BaseExpressionRef ArithmeticOperationsImplementation<T>::add_only_integers() const {
	// sums an all MachineInteger/BigInteger expression

	const T &self = this->expr();
	mpint result(0);

	for (auto value : self.template primitives<mpint>()) {
		result += value;
	}

	return from_primitive(result.to_primitive());
}

template<typename T>
BaseExpressionRef ArithmeticOperationsImplementation<T>::add_only_machine_reals() const {
	// sums an all MachineReal expression

	const T &self = this->expr();
	machine_real_t result = 0.;

	for (auto value : self.template primitives<machine_real_t>()) {
		result += value;
	}

	return from_primitive(result);
}

template<typename T>
BaseExpressionRef ArithmeticOperationsImplementation<T>::add_machine_inexact() const {
	const T &self = this->expr();

	// create an array to store all the symbolic arguments which can't be evaluated.
	std::vector<BaseExpressionRef> symbolics;
	symbolics.reserve(self.size());

	machine_real_t sum = 0.0;
	for (auto leaf : self.leaves()) {
		auto type = leaf->type();
		switch(type) {
			case MachineIntegerType:
				sum += static_cast<machine_real_t>(
					boost::static_pointer_cast<const MachineInteger>(leaf)->value);
				break;
			case BigIntegerType:
				sum += boost::static_pointer_cast<const BigInteger>(leaf)->value.get_d();
				break;
			case MachineRealType:
				sum += boost::static_pointer_cast<const MachineReal>(leaf)->value;
				break;
			case BigRealType:
				sum += boost::static_pointer_cast<const BigReal>(leaf)->_value.toDouble(MPFR_RNDN);
				break;
			case RationalType:
				sum += boost::static_pointer_cast<const Rational>(leaf)->value.get_d();
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

#endif //CMATHICS_ARITHMETIC_IMPL_H_H
