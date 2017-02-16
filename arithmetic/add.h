#ifndef CMATHICS_ADD_H
#define CMATHICS_ADD_H

#include "../core/runtime.h"

BaseExpressionRef add(const Expression *expr, const Evaluation &evaluation);

class PlusNRule : public AtLeastNRule<3> {
public:
	using AtLeastNRule<3>::AtLeastNRule;

	virtual optional<BaseExpressionRef> try_apply(
		const Expression *expr, const Evaluation &evaluation) const;
};

#endif //CMATHICS_ADD_H
