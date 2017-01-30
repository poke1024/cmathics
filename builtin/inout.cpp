#include "inout.h"

/*
 * class Print(Builtin):
    """
    <dl>
    <dt>'Print[$expr$, ...]'
        <dd>prints each $expr$ in string form.
    </dl>

    >> Print["Hello world!"]
     | Hello world!
    >> Print["The answer is ", 7 * 6, "."]
     | The answer is 42.

    #> Print["\[Mu]"]
     | μ
    #> Print["μ"]
     | μ
    """

    def apply(self, expr, evaluation):
        'Print[expr__]'

        expr = expr.get_sequence()
        expr = Expression('Row', Expression('List', *expr))
        evaluation.print_out(expr)
        return Symbol('Null')

 */

class Print : public Builtin {
public:
    static constexpr const char *name = "Print";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'Print[$expr$, ...]'
        <dd>prints each $expr$ in string form.
    </dl>

    >> Print["Hello world!"]
     | Hello world!
    >> Print["The answer is ", 7 * 6, "."]
     | The answer is 42.

    #> Print["\[Mu]"]
     | μ
    #> Print["μ"]
     | μ
	)";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        builtin(&Print::apply);
    }

    inline BaseExpressionRef apply(
        const ExpressionPtr expr,
        const Evaluation &evaluation) {

        evaluation.print_out(expression(evaluation.Row, expr->clone(evaluation.List)));
        return evaluation.Null;
    }
};

class FullForm : public Builtin {
public:
	static constexpr const char *name = "FullForm";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'FullForm[$expr$]'
        <dd>displays the underlying form of $expr$.
    </dl>

    >> FullForm[a + b * c]
     = Plus[a, Times[b, c]]
    >> FullForm[2/3]
     = Rational[2, 3]
    >> FullForm["A string"]
     = "A string"
	)";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
	}
};

class Row : public Builtin {
public:
    static constexpr const char *name = "Row";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'Row[{$expr$, ...}]'
        <dd>formats several expressions inside a 'RowBox'.
    </dl>
	)";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        builtin("MakeBoxes[Row[{items___}, sep_], f_]", &Row::apply);
    }

    inline BaseExpressionRef apply(
        const BaseExpressionPtr items,
        const BaseExpressionPtr sep,
        const BaseExpressionPtr f,
        const Evaluation &evaluation) {

        return BaseExpressionRef();
    }
};

void Builtins::InOut::initialize() {
    add<Print>();
	add<FullForm>();
    add<Row>();
}
