#include "options.h"

inline bool is_option(const BaseExpressionPtr x) {
    if (x->type() != ExpressionType) {
        return false;
    }

    switch (x->as_expression()->head()->extended_type()) {
        case SymbolRule:
            return true;
        case SymbolRuleDelayed:
            return x->as_expression()->size() == 2;
        default:
            return false;
    }
};

inline bool option_q(const BaseExpressionPtr expr) {
    if (expr->type() == ExpressionType &&
        expr->as_expression()->head()->extended_type() == SymbolList) {
        return expr->as_expression()->with_slice(
            [] (const auto &slice) {
                for (const auto leaf : slice) {
                    if (!is_option(leaf.get())) {
                        return false;
                    }
                }
                return true;
            });
    } else {
        return is_option(expr);
    }
};

class OptionQ : public Builtin {
public:
    static constexpr const char *name = "OptionQ";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'OptionQ[$expr$]'
        <dd>returns 'True' if $expr$ has the form of a valid option
        specification.
    </dl>

    Examples of option specifications:
    >> OptionQ[a -> True]
     = True
    >> OptionQ[a :> True]
     = True
    >> OptionQ[{a -> True}]
     = True
    >> OptionQ[{a :> True}]
     = True

    'OptionQ' returns 'False' if its argument is not a valid option
    specification:
    >> OptionQ[x]
     = False
	)";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        builtin(&OptionQ::apply);
    }

    inline BaseExpressionRef apply(
        BaseExpressionPtr expr,
        const Evaluation &evaluation) {

        return evaluation.Boolean(option_q(expr));
    }
};

class NotOptionQ : public Builtin {
public:
    static constexpr const char *name = "NotOptionQ";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'NotOptionQ[$expr$]'
        <dd>returns 'True' if $expr$ does not have the form of a valid
        option specification.
    </dl>

    >> NotOptionQ[x]
     = True
    >> NotOptionQ[2]
     = True
    >> NotOptionQ["abc"]
     = True

    >> NotOptionQ[a -> True]
     = False
	)";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        builtin(&NotOptionQ::apply);
    }

    inline BaseExpressionRef apply(
        BaseExpressionPtr expr,
        const Evaluation &evaluation) {

        return evaluation.Boolean(!option_q(expr));
    }
};

void Builtins::Options::initialize() {
    Unit::add<OptionQ>();
    Unit::add<NotOptionQ>();
}
