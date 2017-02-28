#include "numeric.h"

class NumericQ : public Builtin {
public:
	static constexpr const char *name = "NumericQ";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'NumericQ[$expr$]'
        <dd>tests whether $expr$ represents a numeric quantity.
    </dl>

    >> NumericQ[2]
     = True
    >> NumericQ[Sqrt[Pi]]
     = True
    >> NumberQ[Sqrt[Pi]]
     = False
	)";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&NumericQ::apply);
	}

	inline BaseExpressionRef apply(
		BaseExpressionPtr expr,
		const Evaluation &evaluation) {

		return evaluation.Boolean(expr->is_numeric());
	}
};

void Builtins::Numeric::initialize() {
	add<NumericQ>();
}
