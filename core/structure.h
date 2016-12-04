#ifndef CMATHICS_STRUCTURE_H
#define CMATHICS_STRUCTURE_H

#include "types.h"
#include "operations.h"

class StructureOperations {
public:
	virtual ExpressionRef iterate(
		Symbol *iterator, const BaseExpressionRef &expr, const Evaluation &evaluation) const = 0;

	virtual BaseExpressionRef replace_slots(
		const BaseExpressionRef *slots, size_t n_slots, const Evaluation &evaluation) const = 0;

	virtual BaseExpressionRef replace_vars(Name name) const = 0;
};

template<typename T>
class StructureOperationsImplementation :
	virtual public StructureOperations,
	virtual public OperationsImplementation<T> {
public:
	virtual ExpressionRef iterate(
		Symbol *iterator, const BaseExpressionRef &expr, const Evaluation &evaluation) const;

	virtual BaseExpressionRef replace_slots(
		const BaseExpressionRef *slots, size_t n_slots, const Evaluation &evaluation) const;

	virtual BaseExpressionRef replace_vars(Name name) const;
};

#endif //CMATHICS_STRUCTURE_H
