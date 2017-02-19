#include "inout.h"


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

        assert(items->is_expression()); // must be a Sequence
        const ExpressionPtr items_seq = items->as_expression();

        const BaseExpressionRef MakeBoxes = evaluation.MakeBoxes;

        UnsafeBaseExpressionRef good_sep;
        if (sep->is_string()) {
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
                items_seq->n_leaves<1>()[0],
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

    BaseExpressionRef parenthesize_unpackaged(
        machine_integer_t precedence,
        const BaseExpressionRef &leaf,
        const BaseExpressionRef &leaf_boxes,
        bool when_equal,
        const Evaluation &evaluation) const {

        if (!leaf->is_expression()) {
            return leaf_boxes;
        }

        optional<machine_integer_t> leaf_precedence;

        switch (leaf->as_expression()->head()->symbol()) {
            case S::Infix:
            case S::Prefix:
            case S::Postfix:
                if (leaf->as_expression()->size() >= 3) {
                    leaf_precedence = leaf->as_expression()->leaf(2)->get_int_value();
                }
                break;

            case S::PrecedenceForm:
                if (leaf->as_expression()->size() == 2) {
                    leaf_precedence = leaf->as_expression()->leaf(1)->get_int_value();
                }
                break;

            default:
                // FIXME get builtin precedence
                break;
        }

        if (leaf_precedence) {
            if (precedence > *leaf_precedence ||
                (when_equal && precedence == *leaf_precedence)) {
                return expression(evaluation.RowBox, expression(
                    evaluation.List, m_parentheses[0][0], leaf_boxes, m_parentheses[0][1]));
            }
        }

        return leaf_boxes;
    }

    BaseExpressionRef parenthesize(
        const optional<machine_integer_t> &precedence,
        const BaseExpressionRef &leaf,
        const BaseExpressionRef &leaf_boxes,
        bool when_equal,
        const Evaluation &evaluation) const {

        if (!precedence) {
            return leaf_boxes;
        }

        UnsafeBaseExpressionRef unpackaged = leaf;

        while (unpackaged->is_expression() &&
            unpackaged->as_expression()->head()->symbol() == S::HoldForm &&
            unpackaged->as_expression()->size() == 1) {

            unpackaged = unpackaged->as_expression()->n_leaves<1>()[0];
        }

        return parenthesize_unpackaged(
            *precedence, unpackaged, leaf_boxes, when_equal, evaluation);
    }

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        builtin("MakeBoxes[expr_]", "MakeBoxes[expr, StandardForm]");

        builtin("MakeBoxes[FullForm[expr_], StandardForm|TraditionalForm|OutputForm]",
		    "StyleBox[MakeBoxes[expr, FullForm], ShowStringCharacters->True]");

        builtin(R"(
            MakeBoxes[Infix[expr_, h_, prec_:None, grouping_:None],
            f:StandardForm|TraditionalForm|OutputForm|InputForm]
            )", &MakeBoxes::apply_infix);

        builtin(R"(
            MakeBoxes[(p:Prefix|Postfix)[expr_, h_, prec_:None],
            f:StandardForm|TraditionalForm|OutputForm|InputForm]
            )", &MakeBoxes::apply_postprefix);

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

        if (expr->is_expression()) {
            const size_t n = expr->as_expression()->size();
            const size_t m = 1 /* head */ + 2 /* parentheses */ + (n >= 1 ? 1 : 0);

            const auto generate = [this, expr, form, &evaluation] (auto &store) {
                CachedBaseExpressionRef *parentheses;

                if (form->symbol() == S::TraditionalForm) {
                    parentheses = m_parentheses[0];
                } else {
                    parentheses = m_parentheses[1];
                }

                store(expression(evaluation.MakeBoxes, expr->as_expression()->head(), form));
                store(parentheses[0]);

                const size_t n = expr->as_expression()->size();

                if (n > 1) {
                    const CachedBaseExpressionRef *sep;

                    switch (form->symbol()) {
	                    case S::InputForm:
	                    case S::OutputForm:
	                    case S::FullForm:
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
                        expr->as_expression()->n_leaves<1>()[0],
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

    inline BaseExpressionRef apply_postprefix(
        BaseExpressionPtr p,
        BaseExpressionPtr expr,
        BaseExpressionPtr h,
        BaseExpressionPtr precedence,
        BaseExpressionPtr form,
        const Evaluation &evaluation) {

        if (expr->is_expression() &&
            expr->as_expression()->size() == 1) {

            UnsafeBaseExpressionRef new_h;

            if (!h->is_string()) {
                new_h = expression(evaluation.MakeBoxes, h, form);
                h = new_h.get();
            }

            const BaseExpressionRef pure_leaf =
                expr->as_expression()->n_leaves<1>()[0];

            const BaseExpressionRef leaf = parenthesize(
                precedence->get_int_value(),
                pure_leaf,
                expression(evaluation.MakeBoxes, pure_leaf, form),
                true,
                evaluation);

            UnsafeBaseExpressionRef list;

            if (p->symbol() == S::Postfix) {
                list = expression(evaluation.List, leaf, h);
            } else {
                list = expression(evaluation.List, h, leaf);
            }

            return expression(evaluation.RowBox, list);
        } else {
            return expression(evaluation.MakeBoxes, expr, form);
        }
    }

    inline BaseExpressionRef apply_infix(
        BaseExpressionPtr expr,
        BaseExpressionPtr h,
        BaseExpressionPtr precedence,
        BaseExpressionPtr grouping,
        BaseExpressionPtr form,
        const Evaluation &evaluation) {

	    if (!expr->is_expression()) {
		    return expression(evaluation.MakeBoxes, expr, form);
	    }

        auto get_op = [&form, &evaluation] (const BaseExpressionRef &op) -> BaseExpressionRef {
            if (!op->is_string()) {
                return expression(evaluation.MakeBoxes, op, form);
            } else {
	            const String * const s = op->as_string();
				const size_t n = s->length();

                switch (form->symbol())
	                case S::InputForm: {
	                    if (n == 1) {
		                    const auto c = s->ascii_char_at(0);
		                    if (c == '*' || c == '^') {
			                    break;
		                    }
	                    }
	                    // fallthrough

	                case S::OutputForm: {
                        if (n > 0 && s->ascii_char_at(0) != ' ' && s->ascii_char_at(n - 1) != ' ') {
	                        std::ostringstream t;
	                        t << " " << op->as_string()->utf8() << " ";
                            return Pool::String(t.str());
                        }
                        break;
                    }

                    default:
                        break;
                }

                return op;
            }
        };

	    ExpressionPtr t_expr = expr->as_expression();

	    const size_t n = t_expr->size();
	    if (n > 1) {
            UnsafeExpressionRef ops;

			if (h->is_expression() &&
                h->as_expression()->head()->symbol() == S::List &&
                h->as_expression()->size() == n - 1) {

                ops = expression(evaluation.List, sequential([&h, &get_op, n] (auto &store) {
                    return h->as_expression()->with_slice([&store, &get_op, n] (const auto &slice) {
                        for (size_t i = 0; i < n - 1; i++) {
                            store(get_op(slice[i]));
                        }
                    });
                }, n - 1));
            } else {
                const BaseExpressionRef h2 = get_op(h);

                ops = expression(evaluation.List, sequential([n, &h2] (auto &store) {
                    for (size_t i = 0; i < n - 1; i++) {
                        store(BaseExpressionRef(h2));
                    }
                }, n - 1));
            }

            const auto precedence_int = precedence->get_int_value();

            return ops->with_slice([this, t_expr, n, precedence_int, grouping, form, &evaluation] (const auto &ops) {
                return t_expr->with_slice([this, n, &ops, precedence_int, grouping, form, &evaluation] (const auto &leaves) {
                    return expression(evaluation.RowBox, expression(evaluation.List,
                        sequential([this, n, &ops, &leaves, precedence_int, grouping, form, &evaluation] (auto &store) {
                            for (size_t i = 0; i < n; i++) {
                                if (i > 0) {
                                    store(BaseExpressionRef(ops[i - 1]));
                                }

                                bool parenthesized;

                                switch (grouping->symbol()) {
	                                case S::NonAssociative:
                                        parenthesized = true;
                                        break;
	                                case S::Left:
                                        parenthesized = i > 0;
                                        break;
	                                case S::Right:
                                        parenthesized = i == 0;
                                        break;
                                    default:
                                        parenthesized = false;
                                        break;
                                }

                                BaseExpressionRef leaf = leaves[i];

                                store(parenthesize(
                                    precedence_int,
                                    leaf,
                                    expression(evaluation.MakeBoxes, leaf, form),
                                    parenthesized,
                                    evaluation));
                            }
                        }, (n - 1) + n)));
                    });
            });
	    } else if (n == 1) {
		    return expression(evaluation.MakeBoxes, t_expr->n_leaves<1>()[0], form);
	    } else {
		    return expression(evaluation.MakeBoxes, expr, form);
	    }
    }
};

/*struct NumberFormOptions {
	BaseExpressionPtr DigitBlock;
	BaseExpressionPtr ExponentFunction;
	BaseExpressionPtr ExponentStep;
	BaseExpressionPtr NumberFormat;
	BaseExpressionPtr NumberMultiplier;
	BaseExpressionPtr NumberPadding;
	BaseExpressionPtr NumberPoint;
	BaseExpressionPtr NumberSeparator;
	BaseExpressionPtr NumberSigns;
	BaseExpressionPtr SignPadding;
};*/

template<>
class OptionsDefinitions<NumberFormOptions> {
private:
	const NumberFormatter &m_formatter;

public:
	inline OptionsDefinitions(Definitions &definitions) :
		m_formatter(definitions.number_form) {
	}

	inline const NumberFormOptions &defaults() const {
		return m_formatter.defaults();
	}

	inline bool set(
		NumberFormOptions &options,
		const SymbolPtr key,
		const BaseExpressionRef &value) const {

		m_formatter.parse_option(options, key, value);
		return true;
	}
};

class NumberForm : public Builtin {
public:
	static constexpr const char *name = "NumberForm";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'NumberForm[$expr$, $n$]'
        <dd>prints a real number $expr$ with $n$-digits of precision.
    <dt>'NumberForm[$expr$, {$n$, $f$}]'
        <dd>prints with $n$-digits and $f$ digits to the right of the decimal point.
    </dl>
	)";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(
			"MakeBoxes[NumberForm[expr_, OptionsPattern[NumberForm]], form:StandardForm|TraditionalForm|OutputForm]",
			&NumberForm::apply);
	}

	inline BaseExpressionRef apply(
		BaseExpressionPtr expr,
		BaseExpressionPtr form,
		const NumberFormOptions &options,
		const Evaluation &evaluation) {

		optional<size_t> n = 6;

		if (n) {
			optional<SExp> s_exp = expr->to_s_exp(n);

			if (s_exp) {
				return evaluation.definitions.number_form(
					*s_exp, *n, form, options, evaluation);
			}
		}

		return expression(evaluation.MakeBoxes, expr, form);
	}
};

void Builtins::InOut::initialize() {
    add<Print>();
	add<FullForm>();
    add<Row>();
    add<MakeBoxes>();
	add<NumberForm>();
}
