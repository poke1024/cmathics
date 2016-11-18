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

class Less : public QuickBuiltinRule {
public:
	virtual BaseExpressionRef try_apply(
		const ExpressionRef &expr, const Evaluation &evaluation) const;
};

class Greater : public QuickBuiltinRule {
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
	virtual BaseExpressionRef Plus() const = 0;
	virtual BaseExpressionRef Less(const Evaluation &evaluation) const = 0;
	virtual BaseExpressionRef Greater(const Evaluation &evaluation) const = 0;
};

template<typename T>
class ArithmeticOperationsImplementation :
	virtual public ArithmeticOperations,
	virtual public OperationsImplementation<T> {
public:
	virtual BaseExpressionRef Plus() const;
	virtual BaseExpressionRef Less(const Evaluation &evaluation) const;
	virtual BaseExpressionRef Greater(const Evaluation &evaluation) const;
};

#endif
