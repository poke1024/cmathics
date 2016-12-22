#include "assignment.h"

inline BaseExpressionRef assign(
	BaseExpressionPtr lhs,
	BaseExpressionPtr rhs,
	const Evaluation &evaluation) {

	// FIXME this is just a super simple example implementation.

	const Symbol *name = lhs->lookup_name();
	if (name) {
		const_cast<Symbol*>(name)->add_rule(lhs, rhs);
	}

	// f[x_] := f[x - 1] will end up in an inifinite recursion
	// if we do not return Null but rhs here.

	return evaluation.Null;
}

void Builtins::Assignment::initialize() {

	add("SetDelayed",
		Attributes::HoldAll + Attributes::SequenceHold, {
			builtin<2>(assign)
	});

	add("Set",
		Attributes::HoldFirst + Attributes::SequenceHold, {
			builtin<2>(assign)
	});

}
