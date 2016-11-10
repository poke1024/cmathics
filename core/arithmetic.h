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
	virtual BaseExpressionRef add_only_integers() const = 0;
	virtual BaseExpressionRef add_only_machine_reals() const = 0;
	virtual BaseExpressionRef add_machine_inexact() const = 0;
};

template<typename T>
class ArithmeticOperationsImplementation :
		virtual public ArithmeticOperations,
		virtual public OperationsImplementation<T> {
public:
	virtual BaseExpressionRef add_only_integers() const;
	virtual BaseExpressionRef add_only_machine_reals() const;
	virtual BaseExpressionRef add_machine_inexact() const;
};

#endif
