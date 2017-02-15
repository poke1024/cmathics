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

            if (result->symbol() == S::Null && prev_result->symbol() != S::Null) {
                evaluation.predetermined_out = prev_result;
            }
        }

        return result;
    }
};

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

        const auto symbol = cond->symbol();
        if (symbol == S::True) {
            return t->evaluate_or_copy(evaluation);
        } else if (symbol == S::False) {
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

        const auto symbol = cond->symbol();
        if (symbol == S::True) {
            return t->evaluate_or_copy(evaluation);
        } else if (symbol == S::False) {
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

        const auto symbol = cond->symbol();
        if (symbol == S::True) {
            return t->evaluate_or_copy(evaluation);
        } else if (symbol == S::False) {
            return f->evaluate_or_copy(evaluation);
        } else {
            return u->evaluate_or_copy(evaluation);
        }
    }
};

class Switch : public Builtin {
public:
    static constexpr const char *name = "Switch";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'Switch[$expr$, $pattern1$, $value1$, $pattern2$, $value2$, ...]'
        <dd>yields the first $value$ for which $expr$ matches the corresponding $pattern$.
    </dl>

    >> Switch[2, 1, x, 2, y, 3, z]
     = y
    >> Switch[5, 1, x, 2, y]
     = Switch[5, 1, x, 2, y]
    >> Switch[5, 1, x, 2, y, _, z]
     = z
    #> Switch[2, 1]
     : Switch called with 2 arguments. Switch must be called with an odd number of arguments.
     = Switch[2, 1]
	)";

public:
    using Builtin::Builtin;

    static constexpr auto attributes =
        Attributes::HoldRest;

    void build(Runtime &runtime) {
        builtin(&Switch::apply);

        m_symbol->add_message(
            "argct",
            "Switch called with `1` arguments. Switch must be called with an odd number of arguments.",
            runtime.definitions());
    }

    inline BaseExpressionRef apply(
        const BaseExpressionRef *args,
        size_t n,
        const Evaluation &evaluation) {

        if ((n + 1) % 2) {
            evaluation.message(m_symbol, "argct", Pool::MachineInteger(n));
            return BaseExpressionRef();
        }

        const BaseExpressionRef selector = args[0];

        for (size_t i = 1; i < n; i += 2) {
            const BaseExpressionRef &pattern = args[i];
            const Matcher matcher(pattern);

            if (matcher(selector, evaluation)) {
                const BaseExpressionRef &value = args[i + 1];
                return value;
            }
        }

        return BaseExpressionRef();
    }
};

void Builtins::Control::initialize() {
    add<If>();
    add<CompoundExpression>();
    add<Switch>();
}
