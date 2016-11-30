#include "control.h"

void Builtin::Control::initialize() {

	add("If",
		Attributes::HoldRest, {
			builtin<2>([](
					const BaseExpressionRef &cond,
					const BaseExpressionRef &t,
					const Evaluation &evaluation) {

				const ExtendedType type = cond->extended_type();
				if (type == SymbolTrue) {
					const BaseExpressionRef r = t->evaluate(evaluation);
					return r ? r : t;
				} else if (type == SymbolFalse) {
						const Definitions &definitions = evaluation.definitions;
						return BaseExpressionRef(definitions.symbols().Null);
					} else {
						return BaseExpressionRef();
					}
			}
			),
			builtin<3>([](
					const BaseExpressionRef &cond,
					const BaseExpressionRef &t,
					const BaseExpressionRef &f,
					const Evaluation &evaluation) {

				const ExtendedType type = cond->extended_type();
				if (type == SymbolTrue) {
					const BaseExpressionRef r = t->evaluate(evaluation);
					return r ? r : t;
				} else if (type == SymbolFalse) {
						const BaseExpressionRef r = f->evaluate(evaluation);
						return r ? r : f;
					} else {
						return BaseExpressionRef();
					}
			}
			),
			builtin<4>([](
					const BaseExpressionRef &cond,
					const BaseExpressionRef &t,
					const BaseExpressionRef &f,
					const BaseExpressionRef &u,
					const Evaluation &evaluation) {

				const ExtendedType type = cond->extended_type();
				if (type == SymbolTrue) {
					const BaseExpressionRef r = t->evaluate(evaluation);
					return r ? r : t;
				} else if (type == SymbolFalse) {
						const BaseExpressionRef r = f->evaluate(evaluation);
						return r ? r : f;
					} else {
						const BaseExpressionRef r = u->evaluate(evaluation);
						return r ? r : u;
					}
			}
		)
	});
}
