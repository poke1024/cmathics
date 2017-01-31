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
        builtin("MakeBoxes[Row[{items___}, sep_:\"\"], f_]", &Row::apply);
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

class MakeBoxes : public Builtin {
public:
    static constexpr const char *name = "MakeBoxes";

    static constexpr auto attributes = Attributes::HoldAllComplete;

    static constexpr const char *docs = R"(
    <dl>
    <dt>'MakeBoxes[$expr$]'
        <dd>is a low-level formatting primitive that converts $expr$
        to box form, without evaluating it.
    <dt>'\( ... \)'
        <dd>directly inputs box objects.
    </dl>
	)";

private:
    CachedBaseExpressionRef m_parentheses[2][2];
    CachedBaseExpressionRef m_separators[2];

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
	    rule("MakeBoxes[expr_]", "MakeBoxes[expr, StandardForm]");

        builtin(&MakeBoxes::apply);

        m_parentheses[0][0].initialize(Pool::String("("));
        m_parentheses[0][1].initialize(Pool::String(")"));

        m_parentheses[1][0].initialize(Pool::String("["));
        m_parentheses[1][1].initialize(Pool::String("]"));

        m_separators[0].initialize(Pool::String(", "));
        m_separators[1].initialize(Pool::String(","));
    }

    inline BaseExpressionRef apply(
        BaseExpressionPtr expr,
        BaseExpressionPtr form,
        const Evaluation &evaluation) {

        if (expr->type() == ExpressionType) {
            const size_t n = expr->as_expression()->size();
            const size_t m = 1 /* head */ + 2 /* parentheses */ + (n >= 1 ? 1 : 0);

            const auto generate = [this, expr, form, &evaluation] (auto &store) {
                CachedBaseExpressionRef *parentheses;

                if (form->extended_type() == SymbolTraditionalForm) {
                    parentheses = m_parentheses[0];
                } else {
                    parentheses = m_parentheses[1];
                }

                store(expression(evaluation.MakeBoxes, expr->as_expression()->head(), form));
                store(parentheses[0]);

                const size_t n = expr->as_expression()->size();

                if (n > 1) {
                    const CachedBaseExpressionRef *sep;

                    switch (form->extended_type()) {
                        case SymbolInputForm:
                        case SymbolOutputForm:
                        case SymbolFullForm:
                            sep = &m_separators[0];
                            break;
                        default:
                            sep = &m_separators[1];
                            break;
                    }

                    store(expr->as_expression()->with_slice([sep, form, &evaluation] (const auto &slice) {
                        const size_t n = slice.size();
                        return expression(evaluation.RowBox,
                            expression(evaluation.List, sequential([sep, &slice, form, &evaluation] (auto &store) {
                               const size_t n = slice.size();
                                for (size_t i = 0; i < n; i++) {
                                    if (i > 0) {
                                        store(BaseExpressionRef(*sep));
                                    }
                                    store(expression(
                                        evaluation.MakeBoxes,
                                        BaseExpressionRef(slice[i]),
                                        form));
                                }
                            }, n + /* seps */ (n - 1))));
                        }));
                } else if (n == 1) {
                    store(expression(
                        evaluation.MakeBoxes,
                        expr->as_expression()->static_leaves<1>()[0],
                        form));
                } else {
                    // nothing
                }

                store(parentheses[1]);
            };

            return expression(evaluation.RowBox, expression(
                evaluation.List, sequential(generate, m)));
        } else {
            return expr->make_boxes(form, evaluation);
        }
    }
};

void Builtins::InOut::initialize() {
    add<Print>();
	add<FullForm>();
    add<Row>();
    add<MakeBoxes>();
}
