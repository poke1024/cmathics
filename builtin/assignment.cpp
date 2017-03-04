#include "assignment.h"

inline BaseExpressionRef assign(
	ExpressionPtr,
	BaseExpressionPtr lhs,
	BaseExpressionPtr rhs,
	const Evaluation &evaluation) {

	// FIXME this is just a super simple example implementation.

	const Symbol *name = lhs->lookup_name();
	if (name) {
		const_cast<Symbol*>(name)->state().add_rule(
            lhs, rhs, evaluation);
	}

	// f[x_] := f[x - 1] will end up in an inifinite recursion
	// if we do not return Null but rhs here.

	return evaluation.Null;
}

class Values : public Builtin {
protected:
	bool check_symbol(BaseExpressionPtr symbol, const Evaluation &evaluation) {
		if (!symbol->is_symbol()) {
			evaluation.message(m_symbol, "sym", symbol, MachineInteger::construct(1));
			return false;
		} else {
			return true;
		}
	}

	BaseExpressionRef values(const Rules *rules, const Evaluation &evaluation) {
		if (!rules) {
			return evaluation.definitions.empty_list;
		}
		TemporaryRefVector leaves;
		for (const RuleEntry &entry : *rules) {
			UnsafeBaseExpressionRef pattern = entry.pattern();
			const BaseExpressionRef &rhs = entry.rule()->rhs();
			if (!pattern->has_form(S::HoldPattern, 1)) {
				pattern = expression(evaluation.HoldPattern, pattern);
			}
			leaves.push_back(expression(evaluation.RuleDelayed, pattern, rhs));
		}
		return leaves.to_expression(evaluation.List);
	}

public:
	using Builtin::Builtin;
};

class DownValues : public Values {
public:
	static constexpr const char *name = "DownValues";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'DownValues[$symbol$]'
        <dd>gives the list of downvalues associated with $symbol$.
    </dl>

    'DownValues' uses 'HoldPattern' and 'RuleDelayed' to protect the
    downvalues from being evaluated. Moreover, it has attribute
    'HoldAll' to get the specified symbol instead of its value.

    >> f[x_] := x ^ 2
    >> DownValues[f]
     = {HoldPattern[f[x_]] :> x ^ 2}

    Mathics will sort the rules you assign to a symbol according to
    their specificity. If it cannot decide which rule is more special,
    the newer one will get higher precedence.
    >> f[x_Integer] := 2
    >> f[x_Real] := 3
    >> DownValues[f]
     = {HoldPattern[f[x_Real]] :> 3, HoldPattern[f[x_Integer]] :> 2, HoldPattern[f[x_]] :> x ^ 2}
    >> f[3]
     = 2
    >> f[3.]
     = 3
    >> f[a]
     = a ^ 2
    )";

	static constexpr auto attributes = Attributes::HoldAll;

	void build(Runtime &runtime) {
		builtin(&DownValues::apply);
	}

	inline BaseExpressionRef apply(
		const BaseExpressionPtr symbol,
		const Evaluation &evaluation) {

		if (!check_symbol(symbol, evaluation)) {
			return BaseExpressionRef();
		}
		return values(
			symbol->as_symbol()->state().down_rules(),
			evaluation);
	}

	using Values::Values;
};

void Builtins::Assignment::initialize() {

	add("SetDelayed",
		Attributes::HoldAll + Attributes::SequenceHold, {
			builtin<2>(assign)
	});

	add("Set",
		Attributes::HoldFirst + Attributes::SequenceHold, {
			builtin<2>(assign)
	});

	add<DownValues>();
}
