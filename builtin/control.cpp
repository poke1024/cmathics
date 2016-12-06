#include "control.h"

void Builtins::Control::initialize() {

	add("If", Attributes::HoldRest, {
		builtin<2>([](
			BaseExpressionPtr cond,
			BaseExpressionPtr t,
			const Evaluation &evaluation) {

			const ExtendedType type = cond->extended_type();
			if (type == SymbolTrue) {
				return t->evaluate_or_copy(evaluation);
			} else if (type == SymbolFalse) {
				return BaseExpressionRef(evaluation.Null);
			} else {
				return BaseExpressionRef();
			}
		}
		),
		builtin<3>([](
			BaseExpressionPtr cond,
			BaseExpressionPtr t,
			BaseExpressionPtr f,
			const Evaluation &evaluation) {

			const ExtendedType type = cond->extended_type();
			if (type == SymbolTrue) {
				return t->evaluate_or_copy(evaluation);
			} else if (type == SymbolFalse) {
				return f->evaluate_or_copy(evaluation);
			} else {
				return BaseExpressionRef();
			}
		}),
		builtin<4>([](
			BaseExpressionPtr cond,
			BaseExpressionPtr t,
			BaseExpressionPtr f,
			BaseExpressionPtr u,
			const Evaluation &evaluation) {

			const ExtendedType type = cond->extended_type();
			if (type == SymbolTrue) {
				return t->evaluate_or_copy(evaluation);
			} else if (type == SymbolFalse) {
				return f->evaluate_or_copy(evaluation);
			} else {
				return u->evaluate_or_copy(evaluation);
			}
		})
	});
}
