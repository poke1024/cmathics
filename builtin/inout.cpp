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
                    leaf_precedence = leaf->as_expression()->leaf(2)->get_machine_int_value();
                }
                break;

            case S::PrecedenceForm:
                if (leaf->as_expression()->size() == 2) {
                    leaf_precedence = leaf->as_expression()->leaf(1)->get_machine_int_value();
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

        builtin("MakeBoxes[InputForm[expr_], StandardForm|TraditionalForm|OutputForm]",
            "StyleBox[MakeBoxes[expr, InputForm], ShowStringCharacters->True]");

        builtin(R"(
            MakeBoxes[Infix[expr_, h_, prec_:None, grouping_:None],
            f:StandardForm|TraditionalForm|OutputForm|InputForm]
            )", &MakeBoxes::apply_infix);

        builtin(R"(
            MakeBoxes[(p:Prefix|Postfix)[expr_, h_, prec_:None],
            f:StandardForm|TraditionalForm|OutputForm|InputForm]
            )", &MakeBoxes::apply_postprefix);

        builtin(&MakeBoxes::apply);

        m_parentheses[0][0].initialize(String::construct("("));
        m_parentheses[0][1].initialize(String::construct(")"));

        m_parentheses[1][0].initialize(String::construct("["));
        m_parentheses[1][1].initialize(String::construct("]"));

        m_separators[0].initialize(String::construct(", "));
        m_separators[1].initialize(String::construct(","));
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
                precedence->get_machine_int_value(),
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
                            return String::construct(t.str());
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

            const auto precedence_int = precedence->get_machine_int_value();

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
		const BaseExpressionRef &value,
        const Evaluation &evaluation) const {

		m_formatter.parse_option(
            options, m_formatter.defaults(), key, value, evaluation);
		return true;
	}
};

class OptionsList {
private:
    TempVector m_options;

public:
    OptionsList() {
    }

    inline void add(
        const SymbolPtr key,
        const BaseExpressionRef &value,
        const Evaluation &evaluation) {

        m_options.push_back(expression(evaluation.RuleDelayed, key, value));
    }

    inline BaseExpressionRef to_list(
        const Evaluation &evaluation) const {

        return m_options.to_expression(evaluation.List);
    }

    inline const TempVector &rules() const {
        return m_options;
    }
};

template<>
class OptionsDefinitions<OptionsList> {
private:
    const Definitions &m_definitions;
    const OptionsList m_default_list;

public:
    inline OptionsDefinitions(Definitions &definitions) :
        m_definitions(definitions) {
    }

    // initialize

    inline const OptionsList &defaults() const {
        return m_default_list;
    }

    inline bool set(
        OptionsList &options,
        const SymbolPtr key,
        const BaseExpressionRef &value,
        const Evaluation &evaluation) const {

        options.add(key, value, evaluation);
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

    >> NumberForm[N[Pi], 10]
     = 3.141592654

    >> NumberForm[N[Pi], {10, 5}]
     = 3.14159


    ## Undocumented edge cases
    #> NumberForm[Pi, 20]
     = Pi
    #> NumberForm[2/3, 10]
     = 2 / 3

    ## No n or f
    #> NumberForm[N[Pi]]
     = 3.14159
    #> NumberForm[N[Pi, 20]]
     = 3.1415926535897932385
    #> NumberForm[14310983091809]
     = 14310983091809

    ## Zero case
    #> z0 = 0.0;
    #> z1 = 0.0000000000000000000000000000;
    #> NumberForm[{z0, z1}, 10]
     = {0., 0.×10^-28}
    #> NumberForm[{z0, z1}, {10, 4}]
     = {0.0000, 0.0000×10^-28}

    ## Trailing zeros
    #> NumberForm[1.0, 10]
     = 1.
    #> NumberForm[1.000000000000000000000000, 10]
     = 1.000000000
    #> NumberForm[1.0, {10, 8}]
     = 1.00000000
    #> NumberForm[N[Pi, 33], 33]
     = 3.14159265358979323846264338327950

    ## Correct rounding - see sympy/issues/11472
    #> NumberForm[0.645658509, 6]
     = 0.645659
    #> NumberForm[N[1/7], 30]
     = 0.1428571428571428

    ## Integer case
    #> NumberForm[{0, 2, -415, 83515161451}, 5]
     = {0, 2, -415, 83515161451}
    #> NumberForm[{2^123, 2^123.}, 4, ExponentFunction -> ((#1) &)]
     = {10633823966279326983230456482242756608, 1.063×10^37}
    #> NumberForm[{0, 10, -512}, {10, 3}]
     = {0.000, 10.000, -512.000}

    ## Check arguments
    #> NumberForm[1.5, -4]
     : Formatting specification -4 should be a positive integer or a pair of positive integers.
     = 1.5
    #> NumberForm[1.5, {1.5, 2}]
     : Formatting specification {1.5, 2} should be a positive integer or a pair of positive integers.
     = 1.5
    #> NumberForm[1.5, {1, 2.5}]
     : Formatting specification {1, 2.5} should be a positive integer or a pair of positive integers.
     = 1.5

    ## Right padding
    #> NumberForm[153., 2]
     : In addition to the number of digits requested, one or more zeros will appear as placeholders.
     = 150.
    #> NumberForm[0.00125, 1]
     = 0.001
    #> NumberForm[10^5 N[Pi], {5, 3}]
     : In addition to the number of digits requested, one or more zeros will appear as placeholders.
     = 314160.000
    #> NumberForm[10^5 N[Pi], {6, 3}]
     = 314159.000
    #> NumberForm[10^5 N[Pi], {6, 10}]
     = 314159.0000000000
    #> NumberForm[1.0000000000000000000, 10, NumberPadding -> {"X", "Y"}]
     = X1.000000000

    ## Check options

    ## DigitBlock
    #> NumberForm[12345.123456789, 14, DigitBlock -> 3]
     = 12,345.123 456 789
    #> NumberForm[12345.12345678, 14, DigitBlock -> 3]
     = 12,345.123 456 78
    #> NumberForm[N[10^ 5 Pi], 15, DigitBlock -> {4, 2}]
     = 31,4159.26 53 58 97 9
    #> NumberForm[1.2345, 3, DigitBlock -> -4]
     : Value for option DigitBlock should be a positive integer, Infinity, or a pair of positive integers.
     = 1.2345
    #> NumberForm[1.2345, 3, DigitBlock -> x]
     : Value for option DigitBlock should be a positive integer, Infinity, or a pair of positive integers.
     = 1.2345
    #> NumberForm[1.2345, 3, DigitBlock -> {x, 3}]
     : Value for option DigitBlock should be a positive integer, Infinity, or a pair of positive integers.
     = 1.2345
    #> NumberForm[1.2345, 3, DigitBlock -> {5, -3}]
     : Value for option DigitBlock should be a positive integer, Infinity, or a pair of positive integers.
     = 1.2345

    ## ExponentFunction
    #> NumberForm[12345.123456789, 14, ExponentFunction -> ((#) &)]
     = 1.2345123456789×10^4
    #> NumberForm[12345.123456789, 14, ExponentFunction -> (Null&)]
     = 12345.123456789
    #> y = N[Pi^Range[-20, 40, 15]];
    #> NumberForm[y, 10, ExponentFunction -> (3 Quotient[#, 3] &)]
     =  {114.0256472×10^-12, 3.267763643×10^-3, 93.64804748×10^3, 2.683779414×10^12, 76.91214221×10^18}
    #> NumberForm[y, 10, ExponentFunction -> (Null &)]
     : In addition to the number of digits requested, one or more zeros will appear as placeholders.
     : In addition to the number of digits requested, one or more zeros will appear as placeholders.
     = {0.0000000001140256472, 0.003267763643, 93648.04748, 2683779414000., 76912142210000000000.}

    ## ExponentStep
    #> NumberForm[10^8 N[Pi], 10, ExponentStep -> 3]
     = 314.1592654×10^6
    #> NumberForm[1.2345, 3, ExponentStep -> x]
     : Value of option ExponentStep -> x is not a positive integer.
     = 1.2345
    #> NumberForm[1.2345, 3, ExponentStep -> 0]
     : Value of option ExponentStep -> 0 is not a positive integer.
     = 1.2345
    #> NumberForm[y, 10, ExponentStep -> 6]
     = {114.0256472×10^-12, 3267.763643×10^-6, 93648.04748, 2.683779414×10^12, 76.91214221×10^18}

    ## NumberFormat
    #> NumberForm[y, 10, NumberFormat -> (#1 &)]
     = {1.140256472, 0.003267763643, 93648.04748, 2.683779414, 7.691214221}

    ## NumberMultiplier
    #> NumberForm[1.2345, 3, NumberMultiplier -> 0]
     : Value for option NumberMultiplier -> 0 is expected to be a string.
     = 1.2345
    #> NumberForm[N[10^ 7 Pi], 15, NumberMultiplier -> "*"]
     = 3.14159265358979*10^7

    ## NumberPoint
    #> NumberForm[1.2345, 5, NumberPoint -> ","]
     = 1,2345
    #> NumberForm[1.2345, 3, NumberPoint -> 0]
     : Value for option NumberPoint -> 0 is expected to be a string.
     = 1.2345

    ## NumberPadding
    #> NumberForm[1.41, {10, 5}]
     = 1.41000
    #> NumberForm[1.41, {10, 5}, NumberPadding -> {"", "X"}]
     = 1.41XXX
    #> NumberForm[1.41, {10, 5}, NumberPadding -> {"X", "Y"}]
     = XXXXX1.41YYY
    #> NumberForm[1.41, 10, NumberPadding -> {"X", "Y"}]
     = XXXXXXXX1.41
    #> NumberForm[1.2345, 3, NumberPadding -> 0]
     :  Value for option NumberPadding -> 0 should be a string or a pair of strings.
     = 1.2345
    #> NumberForm[1.41, 10, NumberPadding -> {"X", "Y"}, NumberSigns -> {"-------------", ""}]
     = XXXXXXXXXXXXXXXXXXXX1.41
    #> NumberForm[{1., -1., 2.5, -2.5}, {4, 6}, NumberPadding->{"X", "Y"}]
     = {X1.YYYYYY, -1.YYYYYY, X2.5YYYYY, -2.5YYYYY}

    ## NumberSeparator
    #> NumberForm[N[10^ 5 Pi], 15, DigitBlock -> 3, NumberSeparator -> " "]
     = 314 159.265 358 979
    #> NumberForm[N[10^ 5 Pi], 15, DigitBlock -> 3, NumberSeparator -> {" ", ","}]
     = 314 159.265,358,979
    #> NumberForm[N[10^ 5 Pi], 15, DigitBlock -> 3, NumberSeparator -> {",", " "}]
     = 314,159.265 358 979
    #> NumberForm[N[10^ 7 Pi], 15, DigitBlock -> 3, NumberSeparator -> {",", " "}]
     = 3.141 592 653 589 79×10^7
    #> NumberForm[1.2345, 3, NumberSeparator -> 0]
     : Value for option NumberSeparator -> 0 should be a string or a pair of strings.
     = 1.2345

    ## NumberSigns
    #> NumberForm[1.2345, 5, NumberSigns -> {"-", "+"}]
     = +1.2345
    #> NumberForm[-1.2345, 5, NumberSigns -> {"- ", ""}]
     = - 1.2345
    #> NumberForm[1.2345, 3, NumberSigns -> 0]
     : Value for option NumberSigns -> 0 should be a pair of strings or two pairs of strings.
     = 1.2345

    ## SignPadding
    #> NumberForm[1.234, 6, SignPadding -> True, NumberPadding -> {"X", "Y"}]
     = XXX1.234
    #> NumberForm[-1.234, 6, SignPadding -> True, NumberPadding -> {"X", "Y"}]
     = -XX1.234
    #> NumberForm[-1.234, 6, SignPadding -> False, NumberPadding -> {"X", "Y"}]
     = XX-1.234
    #> NumberForm[-1.234, {6, 4}, SignPadding -> False, NumberPadding -> {"X", "Y"}]
     = X-1.234Y

    ## 1-arg, Option case
    #> NumberForm[34, ExponentFunction->(Null&)]
     = 34

    ## zero padding integer x0.0 case
    #> NumberForm[50.0, {5, 1}]
     = 50.0
    #> NumberForm[50, {5, 1}]
     = 50.0

    ## Rounding correctly
    #> NumberForm[43.157, {10, 1}]
     = 43.2
    #> NumberForm[43.15752525, {10, 5}, NumberSeparator -> ",", DigitBlock -> 1]
     = 4,3.1,5,7,5,3
    #> NumberForm[80.96, {16, 1}]
     = 81.0
    #> NumberForm[142.25, {10, 1}]
     = 142.3
	)";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
        builtin(
            "NumberForm[expr_?ListQ, n_, OptionsPattern[NumberForm]]",
            &NumberForm::apply_list_n);

		builtin(
			"MakeBoxes[NumberForm[expr_, Shortest[n_:Automatic], OptionsPattern[NumberForm]], form:StandardForm|TraditionalForm|OutputForm]",
			&NumberForm::apply);

        message("npad", "Value for option NumberPadding -> `1` should be a string or a pair of strings.");
        message("dblk", "Value for option DigitBlock should be a positive integer, Infinity, or a pair of positive integers.");
        message("npt", "Value for option `1` -> `2` is expected to be a string.");
        message("nsgn", "Value for option NumberSigns -> `1` should be a pair of strings or two pairs of strings.");
        message("nspr", "Value for option NumberSeparator -> `1` should be a string or a pair of strings.");
        message("opttf", "Value of option `1` -> `2` should be True or False.");
        message("estep", "Value of option `1` -> `2` is not a positive integer.");
        message("iprf", "Formatting specification `1` should be a positive integer or a pair of positive integers.");
	}

    inline BaseExpressionRef apply_list_n(
        BaseExpressionPtr expr,
        BaseExpressionPtr n,
        const OptionsList &options,
        const Evaluation &evaluation) {

        assert(expr->is_expression());

        return expr->as_expression()->with_slice([&n, &options, &evaluation] (const auto &slice) {
            return expression(evaluation.List, sequential([&slice, &n, &options, &evaluation] (auto &store) {
                for (auto leaf : slice) {
                    store(expression(
                        evaluation.NumberForm, sequential([&leaf, &n, &options] (auto &store) {
                            store(BaseExpressionRef(leaf));
                            store(BaseExpressionRef(n));
                            for (auto rule : options.rules()) {
                                store(BaseExpressionRef(rule));
                            }
                        })));
                }
            }, slice.size()));
        });
    }

	inline BaseExpressionRef apply(
        BaseExpressionPtr expr,
        BaseExpressionPtr n,
		BaseExpressionPtr form,
		const NumberFormOptions &options,
		const Evaluation &evaluation) {

        const auto fallback = [expr, form, &evaluation] () {
            return expression(evaluation.MakeBoxes, expr, form);
        };

        if (!options.valid) {
            return fallback();
        }

        optional<machine_integer_t> integer_n;
        optional<machine_integer_t> integer_f;

        const auto check_n = [this, n, &integer_n, &evaluation] () {
            if (!integer_n || *integer_n <= 0) {
                evaluation.message(m_symbol, "iprf", n);
                return false;
            } else {
                return true;
            }
        };

        if (n->symbol() == S::Automatic) {
            // ok
        } else if (n->has_form(S::List, 2, evaluation)) {
            const auto * const leaves = n->as_expression()->n_leaves<2>();
            integer_n = leaves[0]->get_machine_int_value();
            integer_f = leaves[1]->get_machine_int_value();

            if (!check_n()) {
                return fallback();
            }

            if (!integer_f || *integer_f < 0) {
                evaluation.message(m_symbol, "iprf", n);
                return fallback();
            }
        } else {
            integer_n = n->get_machine_int_value();
            if (!check_n()) {
                return fallback();
            }
        }

        optional<SExp> s_exp = expr->to_s_exp(integer_n);

        if (!s_exp) {
            return fallback();
        }

        return evaluation.definitions.number_form(
            *s_exp, *integer_n, integer_f, form, options, evaluation);
	}
};

void Builtins::InOut::initialize() {
    add<Print>();
	add<FullForm>();
    add<Row>();
    add<MakeBoxes>();
	add<NumberForm>();
}
