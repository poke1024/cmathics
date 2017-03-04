#include "patterns.h"
#include "arithmetic/binary.h"

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

public:
	using BinaryOperatorBuiltin::BinaryOperatorBuiltin;

	void build(Runtime &runtime) {
		add_binary_operator_formats();
	}

	virtual const char *operator_name() const {
		return ":>";
	}

	virtual int precedence() const {
		return 120;
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
    add<ReplaceAll>();
	add<RuleDelayed>();
	add<MatchQ>();
	add<Verbatim>();
	add<HoldPattern>();
	add<Pattern>();
	add<Blank>();
	add<BlankSequence>();
	add<BlankNullSequence>();
	add<PatternTest>();
	add<Condition>();
}
