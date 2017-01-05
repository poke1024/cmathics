#include "control.h"

class If : public Builtin {
public:
    static constexpr const char *name = "If";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'If[$cond$, $pos$, $neg$]'
        <dd>returns $pos$ if $cond$ evaluates to 'True', and $neg$ if it evaluates to 'False'.
    <dt>'If[$cond$, $pos$, $neg$, $other$]'
        <dd>returns $other$ if $cond$ evaluates to neither 'True' nor 'False'.
    <dt>'If[$cond$, $pos$]'
        <dd>returns 'Null' if $cond$ evaluates to 'False'.
    </dl>

    >> If[1<2, a, b]
     = a
    If the second branch is not specified, 'Null' is taken:
    >> If[1<2, a]
     = a
    #> If[False, a] //FullForm
     = Null

    You might use comments (inside '(*' and '*)') to make the branches of 'If' more readable:
    >> If[a, (*then*) b, (*else*) c];
	)";

public:
    using Builtin::Builtin;

    static constexpr auto attributes =
        Attributes::HoldRest;

    void build(Runtime &runtime) {
        builtin(&If::apply2);
        builtin(&If::apply3);
        builtin(&If::apply4);
    }

    inline BaseExpressionRef apply2(
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

    inline BaseExpressionRef apply3(
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
    }

    inline BaseExpressionRef apply4(
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
    }
};

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
    add<If>();
    add<CompoundExpression>();
}
