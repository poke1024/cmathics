#ifndef ARITHMETIC_H
#define ARITHMETIC_H

#include <experimental/optional>

#include "types.h"
#include "operations.h"
#include "integer.h"
#include "real.h"

BaseExpressionRef Plus(
	const ExpressionRef &expr,
	const Evaluation &evaluation);

BaseExpressionRef Range(
	const BaseExpressionRef &imin,
	const BaseExpressionRef &imax,
	const BaseExpressionRef &di,
	const Evaluation &evaluation);

class ArithmeticOperations {
public:
	virtual BaseExpressionRef add_only_integers() const = 0;
	virtual BaseExpressionRef add_only_machine_reals() const = 0;
};

template<typename T>
class ArithmeticOperationsImplementation :
	virtual public ArithmeticOperations,
	virtual public OperationsImplementation<T> {
public:
	virtual BaseExpressionRef add_only_integers() const {
		// sums all integers in an expression

		mpint result(0);

		// XXX right now the entire computation is done with GMP. This is slower
		// than using machine precision arithmetic but simpler because we don't
		// need to handle overflow. Could be optimised.

		for (auto value : this->template primitives<mpint>()) {
			result += value;
		}

		return from_primitive(result.to_primitive());
	}

	virtual BaseExpressionRef add_only_machine_reals() const {
		machine_real_t result = 0.;

		for (auto value : this->template primitives<machine_real_t>()) {
			result += value;
		}

		return from_primitive(result);
	}};

#endif
