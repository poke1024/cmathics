#ifndef ARITHMETIC_H
#define ARITHMETIC_H

#include "types.h"
#include "operations.h"
#include "rule.h"

class BinaryBuiltinRule : public QuickBuiltinRule {
public:
	virtual MatchSize match_size() const {
		return MatchSize::exactly(2);
	}
};

class Plus2 : public BinaryBuiltinRule {
public:
	virtual BaseExpressionRef try_apply(
		const ExpressionRef &expr, const Evaluation &evaluation) const;
};

class Plus3 : public QuickBuiltinRule {
public:
	virtual BaseExpressionRef try_apply(
		const ExpressionRef &expr, const Evaluation &evaluation) const;

	virtual MatchSize match_size() const {
		return MatchSize::at_least(3);
	}
};

class Less : public BinaryBuiltinRule {
public:
	virtual BaseExpressionRef try_apply(
		const ExpressionRef &expr, const Evaluation &evaluation) const;
};

class Greater : public BinaryBuiltinRule {
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
};

template<typename T>
class ArithmeticOperationsImplementation :
	virtual public ArithmeticOperations,
	virtual public OperationsImplementation<T> {
public:
	virtual BaseExpressionRef Plus() const;
};

#endif
