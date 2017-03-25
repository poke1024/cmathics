#include "patterns.h"
#include "levelspec.tcc"
#include "arithmetic/binary.h"

template<bool Repeated, typename Match>
class DoReplaceAll {
private:
    // see PyMathics Expression.apply_rules

    const Match &m_match;
    const Evaluation &m_evaluation;

    inline BaseExpressionRef leaf(UnsafeBaseExpressionRef expr) {
        if (Repeated) {
            bool changed = false;
            while (true) {
                BaseExpressionRef t = m_match(expr);
                if (!t) {
                    if (changed) {
                        return expr;
                    } else {
                        return BaseExpressionRef();
                    }
                }
                expr = t->evaluate_or_copy(m_evaluation);
                changed = true;
            }
        } else {
            return m_match(expr);
        }
    }

    BaseExpressionRef descend(const BaseExpressionRef &expr) {
        if (expr->is_expression()) {
            // FIXME replace head

            return expr->as_expression()->with_slice_c([this, &expr] (const auto &slice) {
                return conditional_map(
                    keep_head(expr->as_expression()->head()),
                    lambda([this] (const BaseExpressionRef &leaf) {
                        return (*this)(leaf);
                    }),
                    slice,
                    m_evaluation
                );
            });
        } else {
            return BaseExpressionRef();
        }
    }

public:
    DoReplaceAll(
        const Match &match,
        const Evaluation &evaluation) :

        m_match(match), m_evaluation(evaluation) {

    }

    BaseExpressionRef operator()(const BaseExpressionRef &expr) {
        BaseExpressionRef t = leaf(expr);
        if (t) {
            if (!Repeated) {
                return t;
            } else {
                return coalesce(descend(t), t);
            }
        } else {
            return descend(expr);
        }
    }
};

template<typename F>
inline BaseExpressionRef match_and_replace(
	const SymbolRef &name,
	const BaseExpressionPtr expr,
	const BaseExpressionPtr pattern,
	const F &replace,
	const Evaluation &evaluation) {

	try {
		if (pattern->is_list()) {
			return pattern->as_expression()->with_slice_c(
                [expr, pattern, &name, &replace, &evaluation] (const auto &slice) {
                    const size_t n = slice.size();

                    bool any_lists = false;

                    for (size_t i = 0; i < n; i++) {
                        if (slice[i]->is_list()) {
                            any_lists = true;
                        } else if (any_lists) {
                            evaluation.message(name, "rmix", pattern);
                            return BaseExpressionRef();
                        }
                    }

                    if (any_lists) {
                        if (false) { // compatibility mode
                            return BaseExpressionRef(expression(
                                evaluation.List, sequential([expr, &name, &slice] (auto &store) {
                                    const size_t n = slice.size();

                                    for (size_t i = 0; i < n; i++) {
                                        store(expression(name, expr, slice[i]));
                                    }
                                })));
                        } else {
                            const auto recurse = lambda(
                                [expr, &name, &replace, &evaluation] (const BaseExpressionRef &leaf) {
                                    const BaseExpressionRef result =
                                        match_and_replace(name, expr, leaf.get(), replace, evaluation);
                                    if (!result) {
                                        ExpressionRef unevaluated = expression(name, expr, leaf);
                                        unevaluated->set_last_evaluated(evaluation.definitions.version());
                                        return BaseExpressionRef(unevaluated);
                                    } else {
                                        return result;
                                    }
                                });

                            return BaseExpressionRef(conditional_map(
                                keep_head(evaluation.List),
                                recurse,
                                slice,
                                evaluation));
                        }
                    }

                    std::vector<ReplacerRef> replacers;
                    replacers.reserve(n);
                    for (size_t i = 0; i < n; i++) {
                        replacers.push_back(instantiate_replacer<MandatoryRuleForm>(
                            slice[i], replacer_factory(), evaluation));
                    }

                    std::vector<optional<MatchContext>> contexts(n);
                    return replace([&replacers, &contexts, &evaluation] (const BaseExpressionRef &item) {
                        const size_t n = replacers.size();

                        for (size_t i = 0; i < n; i++) {
                            const BaseExpressionRef result =
                                replacers[i]->apply(contexts[i], item, evaluation);
                            if (result) {
                                return result;
                            }
                        }

                        return BaseExpressionRef();
                    });
                });
		} else {
			return instantiate_replacer<MandatoryRuleForm>(
				pattern, immediate_replace<F>(replace, evaluation), evaluation);
		}
	} catch (const EvaluationMessage& message) {
		message(name, evaluation);
		return BaseExpressionRef();
	}
}

struct ReplaceOptions {
    BaseExpressionPtr Heads;

    static OptionsInitializerList meta() {
        return {
            {"Heads", offsetof(ReplaceOptions, Heads), "False"}
        };
    };
};

class Replace : public Builtin {
public:
    static constexpr const char *name = "Replace";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'Replace[$expr$, $x$ -> $y$]'
        <dd>yields the result of replacing $expr$ with $y$ if it
        matches the pattern $x$.
    <dt>'Replace[$expr$, $x$ -> $y$, $levelspec$]'
        <dd>replaces only subexpressions at levels specified through
        $levelspec$.
    <dt>'Replace[$expr$, {$x$ -> $y$, ...}]'
        <dd>performs replacement with multiple rules, yielding a
        single result expression.
    <dt>'Replace[$expr$, {{$a$ -> $b$, ...}, {$c$ -> $d$, ...}, ...}]'
        <dd>returns a list containing the result of performing each
        set of replacements.
    </dl>

    >> Replace[x, {x -> 2}]
     = 2

    By default, only the top level is searched for matches
    >> Replace[1 + x, {x -> 2}]
     = 1 + x

    >> Replace[x, {{x -> 1}, {x -> 2}}]
     = {1, 2}

    Replace stops after the first replacement
    >> Replace[x, {x -> {}, _List -> y}]
     = {}

    Replace replaces the deepest levels first
    >> Replace[x[1], {x[1] -> y, 1 -> 2}, All]
     = x[2]

    By default, heads are not replaced
    >> Replace[x[x[y]], x -> z, All]
     = x[x[y]]

    Heads can be replaced using the Heads option
    >> Replace[x[x[y]], x -> z, All, Heads -> True]
     = z[z[y]]

    Note that heads are handled at the level of leaves
    >> Replace[x[x[y]], x -> z, {1}, Heads -> True]
     = z[x[y]]

    You can use Replace as an operator
    >> Replace[{x_ -> x + 1}][10]
     = 11
	)";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        message("reps", "`1` is not a valid replacement rule.");
        message("rmix", "Elements of `1` are a mixture of lists and nonlists.");

        builtin(
            "Replace[list_, patt_, Shortest[ls_:{0}], OptionsPattern[Replace]]",
            &Replace::apply);

        builtin("Replace[rules_][expr_]", "Replace[expr, rules]");
    }

    inline BaseExpressionRef apply(
        BaseExpressionPtr expr,
        BaseExpressionPtr pattern,
        BaseExpressionPtr ls,
        const ReplaceOptions &options,
        const Evaluation &evaluation) {

        try {
            const Levelspec levelspec(ls);

            // if no match happened, we do not want Replace[x, y], but x.
            return coalesce(match_and_replace(
                m_symbol, expr, pattern, [expr, &levelspec, &options, &evaluation] (const auto &match) {

                    return BaseExpressionRef(std::get<0>(
                        levelspec.walk<UnsafeBaseExpressionRef, Levelspec::NoPosition>(
                            expr,
                            options.Heads->is_true(),
                            [&match] (const BaseExpressionRef &node, Levelspec::NoPosition) {
                                return match(node);
                            },
                            evaluation)));

                }, evaluation), expr);
        } catch (const Levelspec::InvalidError&) {
            evaluation.message(m_symbol, "level", ls);
            return BaseExpressionRef();
        }
    }
};

class ReplaceAll : public BinaryOperatorBuiltin {
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
    >> g[a+b+c,a]/.g[x_+y_,x_]->{x,y}
     = {a, b + c}

    If $rules$ is a list of lists, a list of all possible respective
    replacements is returned:
    >> {a, b} /. {{a->x, b->y}, {a->u, b->v}}
     = {{x, y}, {u, v}}
    The list can be arbitrarily nested:
    >> {a, b} /. {{{a->x, b->y}, {a->w, b->z}}, {a->u, b->v}}
     = {{{x, y}, {w, z}}, {u, v}}
    >> {a, b} /. {{{a->x, b->y}, a->w, b->z}, {a->u, b->v}}
     : Elements of {{a -> x, b -> y}, a -> w, b -> z} are a mixture of lists and nonlists.
     = {{a, b} /. {{a -> x, b -> y}, a -> w, b -> z}, {u, v}}

    ReplaceAll also can be used as an operator:
    >> ReplaceAll[{a -> 1}][{a, b}]
     = {1, b}

    #> a + b /. x_ + y_ -> {x, y}
     = {a, b}

    ReplaceAll stops after the first replacement
    >> ReplaceAll[x, {x -> {}, _List -> y}]
     = {}

    ReplaceAll replaces the shallowest levels first:
    >> ReplaceAll[x[1], {x[1] -> y, 1 -> 2}]
     = y
	)";

	virtual const char *operator_name() const {
		return "/.";
	}

	virtual int precedence() const {
		return 110;
	}

	virtual const char *grouping() const {
		return "Left";
	}

public:
    using BinaryOperatorBuiltin::BinaryOperatorBuiltin;

    void build(Runtime &runtime) {
	    message("reps", "`1` is not a valid replacement rule.");
		message("rmix", "Elements of `1` are a mixture of lists and nonlists.");

        builtin(&ReplaceAll::apply);
        builtin("ReplaceAll[rules_][expr_]", "ReplaceAll[expr, rules]");

	    add_binary_operator_formats();
    }

    inline BaseExpressionRef apply(
        BaseExpressionPtr expr,
        BaseExpressionPtr pattern,
        const Evaluation &evaluation) {

        // if no match happened, we do not want ReplaceAll[x, y], but x.
		BaseExpressionRef new_expr = coalesce(match_and_replace(
		    m_symbol, expr, pattern, [expr, &evaluation] (const auto &match) {
                return DoReplaceAll<false, decltype(match)>(match, evaluation)(expr);
            }, evaluation), expr);

        return new_expr;
    }
};

class ReplaceRepeated : public BinaryOperatorBuiltin {
public:
    static constexpr const char *name = "ReplaceRepeated";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'ReplaceRepeated[$expr$, $x$ -> $y$]'
    <dt>'$expr$ //. $x$ -> $y$'
        <dd>repeatedly applies the rule '$x$ -> $y$' to $expr$ until
        the result no longer changes.
    </dl>

    >> a+b+c //. c->d
     = a + b + d

    Simplification of logarithms:
    >> logrules = {Log[x_ * y_] :> Log[x] + Log[y], Log[x_ ^ y_] :> y * Log[x]};
    >> Log[a * (b * c) ^ d ^ e * f] //. logrules
     = Log[a] + Log[f] + (Log[b] + Log[c]) d ^ e
    'ReplaceAll' just performs a single replacement:
    >> Log[a * (b * c) ^ d ^ e * f] /. logrules
     = Log[a] + Log[f (b c) ^ d ^ e]
	)";

    virtual const char *operator_name() const {
        return "//.";
    }

    virtual int precedence() const {
        return 110;
    }

    virtual const char *grouping() const {
        return "Left";
    }

public:
    using BinaryOperatorBuiltin::BinaryOperatorBuiltin;

    void build(Runtime &runtime) {
        message("reps", "`1` is not a valid replacement rule.");
        message("rmix", "Elements of `1` are a mixture of lists and nonlists.");

        builtin(&ReplaceRepeated::apply);

        add_binary_operator_formats();
    }

    inline BaseExpressionRef apply(
        BaseExpressionPtr expr,
        BaseExpressionPtr pattern,
        const Evaluation &evaluation) {

        // if no match happened, we do not want ReplaceRepeated[x, y], but x.
        return coalesce(match_and_replace(
            m_symbol, expr, pattern, [expr, &evaluation] (const auto &match) {
                return DoReplaceAll<true, decltype(match)>(match, evaluation)(expr);
            }, evaluation), expr);
    }
};

class RuleBuiltin : public BinaryOperatorBuiltin {
public:
	static constexpr const char *name = "Rule";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Rule[$x$, $y$]'
    <dt>'$x$ -> $y$'
        <dd>represents a rule replacing $x$ with $y$.
    </dl>

    >> a+b+c /. c->d
    = a + b + d
    >> {x,x^2,y} /. x->3
     = {3, 9, y}

    #> a /. Rule[1, 2, 3] -> t
     : Rule called with 3 arguments; 2 arguments are expected.
     = a
	)";

	static constexpr auto attributes =
		Attributes::SequenceHold;

	virtual const char *operator_name() const {
		return "->";
	}

	virtual int precedence() const {
		return 120;
	}

	virtual const char *grouping() const {
		return "Right";
	}

public:
	using BinaryOperatorBuiltin::BinaryOperatorBuiltin;

	void build(Runtime &runtime) {
		add_binary_operator_formats();
	}
};


class RuleDelayed : public BinaryOperatorBuiltin {
public:
	static constexpr const char *name = "RuleDelayed";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'RuleDelayed[$x$, $y$]'
    <dt>'$x$ :> $y$'
        <dd>represents a rule replacing $x$ with $y$, with $y$ held
        unevaluated.
    </dl>

    >> Attributes[RuleDelayed]
     = {HoldRest, Protected, SequenceHold}
	)";

	static constexpr auto attributes =
		Attributes::SequenceHold + Attributes::HoldRest;

	virtual const char *operator_name() const {
		return ":>";
	}

	virtual int precedence() const {
		return 120;
	}

public:
	using BinaryOperatorBuiltin::BinaryOperatorBuiltin;

	void build(Runtime &runtime) {
		add_binary_operator_formats();
	}
};

class MatchQ : public Builtin {
public:
	static constexpr const char *name = "MatchQ";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'MatchQ[$expr$, $form$]'
        <dd>tests whether $expr$ matches $form$.
    </dl>

    >> MatchQ[123, _Integer]
     = True
    >> MatchQ[123, _Real]
     = False
    >> MatchQ[_Integer][123]
     = True
	)";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin("MatchQ[form_][expr_]", "MatchQ[expr, form]");
		builtin(&MatchQ::apply);
	}

	inline BaseExpressionRef apply(
		const BaseExpressionPtr expr,
		const BaseExpressionPtr pattern,
		const Evaluation &evaluation) {

		return match(pattern, [expr, &evaluation] (const auto &match) {
			return evaluation.Boolean(match(expr));
		}, evaluation);
	}
};


class Verbatim : public Builtin {
public:
	static constexpr const char *name = "Verbatim";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Verbatim[$expr$]'
        <dd>prevents pattern constructs in $expr$ from taking effect,
        allowing them to match themselves.
    </dl>

    Create a pattern matching 'Blank':
    >> _ /. Verbatim[_]->t
     = t
    >> x /. Verbatim[_]->t
     = x

    Without 'Verbatim', 'Blank' has its normal effect:
    >> x /. _->t
     = t
	)";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
	}
};

class HoldPattern : public Builtin {
public:
	static constexpr const char *name = "HoldPattern";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'HoldPattern[$expr$]'
        <dd>is equivalent to $expr$ for pattern matching, but
        maintains it in an unevaluated form.
    </dl>

    >> HoldPattern[x + x]
     = HoldPattern[x + x]
    >> x /. HoldPattern[x] -> t
     = t

    'HoldPattern' has attribute 'HoldAll':
    >> Attributes[HoldPattern]
     = {HoldAll, Protected}
	)";

	static constexpr auto attributes = Attributes::HoldAll;

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
	}
};


class Pattern : public Builtin {
public:
	static constexpr const char *name = "Pattern";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Pattern[$symb$, $patt$]'
    <dt>'$symb$ : $patt$'
        <dd>assigns the name $symb$ to the pattern $patt$.
    <dt>'$symb$_$head$'
        <dd>is equivalent to '$symb$ : _$head$' (accordingly with '__'
        and '___').
    <dt>'$symb$ : $patt$ : $default$'
        <dd>is a pattern with name $symb$ and default value $default$,
        equivalent to 'Optional[$patt$ : $symb$, $default$]'.
    </dl>

    >> FullForm[a_b]
     = Pattern[a, Blank[b]]
    >> FullForm[a:_:b]
     = Optional[Pattern[a, Blank[]], b]
	)";

	static constexpr auto attributes = Attributes::HoldFirst;

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin("MakeBoxes[Verbatim[Pattern][symbol_Symbol, blank_Blank|blank_BlankSequence|blank_BlankNullSequence], "
		    "f:StandardForm|TraditionalForm|InputForm|OutputForm]", "MakeBoxes[symbol, f] <> MakeBoxes[blank, f]");

		format(
			"Verbatim[Pattern][symbol_, pattern_?(!MatchQ[#, _Blank|_BlankSequence|_BlankNullSequence]&)]",
			"Infix[{symbol, pattern}, \":\", 150, Left]");
	}
};

class Blank : public Builtin {
public:
	static constexpr const char *name = "Blank";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Blank[]'
    <dt>'_'
        <dd>represents any single expression in a pattern.
    <dt>'Blank[$h$]'
    <dt>'_$h$'
        <dd>represents any expression with head $h$.
    </dl>
	)";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin("MakeBoxes[Verbatim[Blank][], f:StandardForm|TraditionalForm|OutputForm|InputForm]",
			"\"_\"");
		builtin("MakeBoxes[Verbatim[Blank][head_Symbol], f:StandardForm|TraditionalForm|OutputForm|InputForm]",
			"\"_\" <> MakeBoxes[head, f]");
	}
};

class BlankSequence : public Builtin {
public:
	static constexpr const char *name = "BlankSequence";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'BlankSequence[]'
    <dt>'__'
        <dd>represents any non-empty sequence of expression leaves in
        a pattern.
    <dt>'BlankSequence[$h$]'
    <dt>'__$h$'
        <dd>represents any sequence of leaves, all of which have head $h$.
    </dl>
	)";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin("MakeBoxes[Verbatim[BlankSequence][], f:StandardForm|TraditionalForm|OutputForm|InputForm]",
		    "\"__\"");
		builtin("MakeBoxes[Verbatim[BlankSequence][head_Symbol], f:StandardForm|TraditionalForm|OutputForm|InputForm]",
	        "\"__\" <> MakeBoxes[head, f]");
	}
};

class BlankNullSequence : public Builtin {
public:
	static constexpr const char *name = "BlankNullSequence";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'BlankNullSequence[]'
    <dt>'___'
        <dd>represents any sequence of expression leaves in a pattern,
        including an empty sequence.
    </dl>
	)";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin("MakeBoxes[Verbatim[BlankNullSequence][], f:StandardForm|TraditionalForm|OutputForm|InputForm]",
	        "\"___\"");
		builtin("MakeBoxes[Verbatim[BlankNullSequence][head_Symbol], f:StandardForm|TraditionalForm|OutputForm|InputForm]",
	        "\"___\" <> MakeBoxes[head, f]");
	}
};

class PatternTest : public BinaryOperatorBuiltin {
public:
	static constexpr const char *name = "PatternTest";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'PatternTest[$pattern$, $test$]'
    <dt>'$pattern$ ? $test$'
        <dd>constrains $pattern$ to match $expr$ only if the
        evaluation of '$test$[$expr$]' yields 'True'.
    </dl>

    >> MatchQ[3, _Integer?(#>0&)]
     = True
    >> MatchQ[-3, _Integer?(#>0&)]
     = False
	)";

public:
	using BinaryOperatorBuiltin::BinaryOperatorBuiltin;

	void build(Runtime &runtime) {
		add_binary_operator_formats();
	}

	virtual const char *operator_name() const {
		return "?";
	}

	virtual int precedence() const {
		return 680;
	}
};

class Alternatives : public BinaryOperatorBuiltin {
public:
    static constexpr const char *name = "Alternatives";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'Alternatives[$p1$, $p2$, ..., $p_i$]'
    <dt>'$p1$ | $p2$ | ... | $p_i$'
        <dd>is a pattern that matches any of the patterns '$p1$, $p2$,
        ...., $p_i$'.
    </dl>

    >> a+b+c+d/.(a|b)->t
     = c + d + 2 t

    Alternatives can also be used for string expressions
    >> StringReplace["0123 3210", "1" | "2" -> "X"]
     = 0XX3 3XX0

    #> StringReplace["h1d9a f483", DigitCharacter | WhitespaceCharacter -> ""]
     = hdaf
	)";

public:
    using BinaryOperatorBuiltin::BinaryOperatorBuiltin;

    void build(Runtime &runtime) {
        add_binary_operator_formats();
    }

    virtual const char *operator_name() const {
        return "|";
    }

    virtual int precedence() const {
        return 160;
    }
};

class Condition : public BinaryOperatorBuiltin {
public:
	static constexpr const char *name = "Condition";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Condition[$pattern$, $expr$]'
    <dt>'$pattern$ /; $expr$'
        <dd>places an additional constraint on $pattern$ that only
        allows it to match if $expr$ evaluates to 'True'.
    </dl>
	)";

	static constexpr auto attributes = Attributes::HoldRest;

public:
	using BinaryOperatorBuiltin::BinaryOperatorBuiltin;

	void build(Runtime &runtime) {
		add_binary_operator_formats();
	}

	virtual const char *operator_name() const {
		return "/;";
	}

	virtual int precedence() const {
		return 130;
	}
};

void Builtins::Patterns::initialize() {
    add<Replace>();
    add<ReplaceAll>();
    add<ReplaceRepeated>();
	add<RuleBuiltin>();
	add<RuleDelayed>();
	add<MatchQ>();
	add<Verbatim>();
	add<HoldPattern>();
	add<Pattern>();
	add<Blank>();
	add<BlankSequence>();
	add<BlankNullSequence>();
	add<PatternTest>();
    add<Alternatives>();
	add<Condition>();
}
