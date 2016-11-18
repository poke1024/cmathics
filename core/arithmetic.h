#ifndef ARITHMETIC_H
#define ARITHMETIC_H

#include "types.h"
#include "operations.h"
#include "rule.h"

class Plus : public QuickBuiltinRule {
public:
	virtual BaseExpressionRef try_apply(
		const ExpressionRef &expr, const Evaluation &evaluation) const;
};

BaseExpressionRef Range(
	const BaseExpressionRef &imin,
	const BaseExpressionRef &imax,
	const BaseExpressionRef &di,
	const Evaluation &evaluation);

class ArithmeticOperations {
public:
	virtual BaseExpressionRef plus() const = 0;
	virtual BaseExpressionRef add_only_integers() const = 0;
	virtual BaseExpressionRef add_only_machine_reals() const = 0;
	virtual BaseExpressionRef add_machine_inexact() const = 0;
};

template<typename T>
class ArithmeticOperationsImplementation :
	virtual public ArithmeticOperations,
	virtual public OperationsImplementation<T> {
public:
	virtual BaseExpressionRef plus() const;
	virtual BaseExpressionRef add_only_integers() const;
	virtual BaseExpressionRef add_only_machine_reals() const;
	virtual BaseExpressionRef add_machine_inexact() const;
};

#endif
