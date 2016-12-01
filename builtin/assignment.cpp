#include "assignment.h"

inline BaseExpressionRef assign(
	const BaseExpressionRef &lhs,
	const BaseExpressionRef &rhs,
	const Evaluation &evaluation) {

	// FIXME this is just a super simple example implementation.

	const Symbol *name = lhs->lookup_name();
	if (name) {
		const_cast<Symbol*>(name)->add_rule(lhs, rhs);
	}

	return rhs; // or evaluation.Null?
}

void Builtin::Assignment::initialize() {

	add("SetDelayed",
		Attributes::HoldAll + Attributes::SequenceHold, {
		builtin<2>(assign)
	});

	add("Set",
		Attributes::HoldFirst + Attributes::SequenceHold, {
		builtin<2>(assign)
	});

}
