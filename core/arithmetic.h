#ifndef ARITHMETIC_H
#define ARITHMETIC_H

#include "types.h"
#include "operations.h"

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
	virtual BaseExpressionRef add_integers() const = 0;
};

template<typename T>
class ArithmeticOperationsImplementation :
	virtual public ArithmeticOperations,
	virtual public OperationsImplementation<T> {
public:
	virtual BaseExpressionRef add_integers() const {
		// sums all integers in an expression

		mpz_class result(0);

		// XXX right now the entire computation is done with GMP. This is slower
		// than using machine precision arithmetic but simpler because we don't
		// need to handle overflow. Could be optimised.

		for (auto value : this->template primitives<mpz_class>()) {
			result += mpz_class(value); // FIXME
		}

		return from_primitive(result);
	}
};

#endif
