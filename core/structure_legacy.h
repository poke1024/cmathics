#ifndef CMATHICS_STRUCTURE_H
#define CMATHICS_STRUCTURE_H

#include "types.h"
#include "operations.h"

class StructureOperations {
public:
	virtual BaseExpressionRef replace_vars(Name name) const = 0;
};

template<typename T>
class StructureOperationsImplementation :
	virtual public StructureOperations,
	virtual public OperationsImplementation<T> {
public:
	virtual BaseExpressionRef replace_vars(Name name) const;
};

#endif //CMATHICS_STRUCTURE_H
