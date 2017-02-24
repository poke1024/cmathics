#include "patterns.h"

template<typename Match>
BaseExpressionRef replace_all(
    const BaseExpressionRef &expr,
    const Match &match,
    const Evaluation &evaluation) {

    BaseExpressionRef result = match(expr);
    if (result) {
        return result;
    }

    if (expr->is_expression()) {
        // FIXME replace head

        return expr->as_expression()->with_slice([&expr, &match, &evaluation] (const auto &slice) {
            return ::apply_conditional_map<UnknownTypeMask>(
                expr->as_expression()->head(),
                false, // is_new_head
                lambda([&match, &evaluation](const BaseExpressionRef &leaf) {
                    return replace_all(leaf, match, evaluation);
                }),
                slice,
                0,
                expr->as_expression()->size(),
                evaluation
            );
        });
    } else {
        return BaseExpressionRef();
    }
}

class ReplaceAll : public Builtin {
public:
    static constexpr const char *name = "ReplaceAll";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'ReplaceAll[$expr$, $x$ -> $y$]'
    <dt>'$expr$ /. $x$ -> $y$'
        <dd>yields the result of replacing all subexpressions of
        $expr$ matching the pattern $x$ with $y$.
    <dt>'$expr$ /. {$x$ -> $y$, ...}'
        <dd>performs replacement with multiple rules, yielding a
        single result expression.
    <dt>'$expr$ /. {{$a$ -> $b$, ...}, {$c$ -> $d$, ...}, ...}'
        <dd>returns a list containing the result of performing each
        set of replacements.
    </dl>

    >> a+b+c /. c->d
     = a + b + d
	)";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        builtin(&ReplaceAll::apply);
    }

    inline BaseExpressionRef apply(
        BaseExpressionPtr expr,
        BaseExpressionPtr pattern,
        const Evaluation &evaluation) {

        return coalesce(match(pattern, [expr, &evaluation] (const auto &match) {
            return replace_all(expr, match, evaluation);
        }, evaluation), expr);
    }
};

void Builtins::Patterns::initialize() {
    Unit::add<ReplaceAll>();
}
