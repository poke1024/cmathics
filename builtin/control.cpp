#include "control.h"

class CompoundExpression : public Builtin {
public:
	static constexpr const char *name = "CompoundExpression";

public:
	using Builtin::Builtin;

    static constexpr auto attributes =
        Attributes::HoldAll + Attributes::ReadProtected;

	void build(Runtime &runtime) {
		builtin(&CompoundExpression::apply);
	}

	inline BaseExpressionRef apply(
        const BaseExpressionRef *leaves,
        size_t n,
        const Evaluation &evaluation) {

		UnsafeBaseExpressionRef result = UnsafeBaseExpressionRef(evaluation.Null);
		UnsafeBaseExpressionRef prev_result;

        for (size_t i = 0; i < n; i++) {
            prev_result = result;

            const BaseExpressionRef &leaf = leaves[i];
            result = leaf->evaluate_or_copy(evaluation);

            if (result->extended_type() == SymbolNull && prev_result->extended_type() != SymbolNull) {
                evaluation.predetermined_out = prev_result;
            }
        }

        return result;
	}
};

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

    add<CompoundExpression>();
}
