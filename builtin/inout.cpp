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
        const BaseExpressionPtr form,
        const Evaluation &evaluation) {

        assert(items->type() == ExpressionType); // must be a Sequence
        ExpressionPtr items_seq = items->as_expression();

        const auto &MakeBoxes = evaluation.MakeBoxes;

        UnsafeBaseExpressionRef good_sep;
        if (sep->type() == StringType) {
            if (sep->as_string()->length() > 0) {
                good_sep = sep;
            }
        } else {
            good_sep = expression(MakeBoxes, sep);
        }

        const size_t n = items_seq->size();
        if (n == 1) {
            return expression(
                MakeBoxes,
                items_seq->static_leaves<1>()[0],
                form);
        } else if (n > 1) {
            const size_t m = good_sep ? n - 1 : 0;

            return expression(evaluation.RowBox,
                expression(evaluation.List, sequential([n, items_seq, &good_sep, &form, &MakeBoxes] (auto &store) {
                    items_seq->with_slice([&store, &good_sep, &form, &MakeBoxes] (const auto &slice) {
                        const size_t n = slice.size();
                        for (size_t i = 0; i < n; i++) {
                            if (i > 0 && good_sep) {
                                store(BaseExpressionRef(good_sep));
                            }
                            store(expression(MakeBoxes, slice[i], form));
                        }
                    });
                }, n + m)));
        } else {
            return expression(evaluation.RowBox, expression(evaluation.List));
        }
    }
};

void Builtins::InOut::initialize() {
    add<Print>();
	add<FullForm>();
    add<Row>();
}
