#ifndef CMATHICS_MUL_H
#define CMATHICS_MUL_H

#include "../core/runtime.h"

inline bool is_minus_1(const BaseExpressionRef &expr) {
	if (expr->is_machine_integer()) {
		return static_cast<const MachineInteger*>(expr.get())->value == -1;
	} else {
		return false;
	}
}

BaseExpressionRef mul(const Expression *expr, const Evaluation &evaluation);

class TimesNRule : public AtLeastNRule<3> {
public:
	using AtLeastNRule<3>::AtLeastNRule;

	virtual optional<BaseExpressionRef> try_apply(
		const Expression *expr, const Evaluation &evaluation) const;
};

#endif //CMATHICS_MUL_H
