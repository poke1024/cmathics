#include "evaluation.h"

class Hold : public Builtin {
public:
	static constexpr const char *name = "Hold";

	static constexpr auto attributes = Attributes::HoldAll;

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Hold[$expr$]'
        <dd>prevents $expr$ from being evaluated.
    </dl>
    >> Attributes[Hold]
     = {HoldAll, Protected}
    )";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
	}
};


class HoldComplete : public Builtin {
public:
	static constexpr const char *name = "HoldComplete";

	static constexpr auto attributes = Attributes::HoldAllComplete;

	static constexpr const char *docs = R"(
    <dl>
    <dt>'HoldComplete[$expr$]'
        <dd>prevents $expr$ from being evaluated, and also prevents
        'Sequence' objects from being spliced into argument lists.
    </dl>
    >> Attributes[HoldComplete]
     = {HoldAllComplete, Protected}
    )";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
	}
};

class HoldForm : public Builtin {
public:
    static constexpr const char *name = "HoldForm";

    static constexpr auto attributes = Attributes::HoldAll;

    static constexpr const char *docs = R"(
    <dl>
    <dt>'HoldForm[$expr$]'
        <dd>is equivalent to 'Hold[$expr$]', but prints as $expr$.
    </dl>

    #> HoldForm[1 + 2 + 3]
     = 1 + 2 + 3

    'HoldForm' has attribute 'HoldAll':
    >> Attributes[HoldForm]
     = {HoldAll, Protected}
    )";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        builtin("MakeBoxes[HoldForm[expr_], f_]", "MakeBoxes[expr, f]");
    }
};

class EvaluateBuiltin : public Builtin {
public:
	static constexpr const char *name = "Evaluate";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Evaluate[$expr$]'
        <dd>forces evaluation of $expr$, even if it occurs inside a
        held argument or a 'Hold' form.
    </dl>

    Create a function $f$ with a held argument:
    >> SetAttributes[f, HoldAll]
    >> f[1 + 2]
     = f[1 + 2]

    'Evaluate' forces evaluation of the argument, even though $f$ has
    the 'HoldAll' attribute:
    >> f[Evaluate[1 + 2]]
     = f[3]

    >> Hold[Evaluate[1 + 2]]
     = Hold[3]
    >> HoldComplete[Evaluate[1 + 2]]
     = HoldComplete[Evaluate[1 + 2]]
    >> Evaluate[Sequence[1, 2]]
     = Sequence[1, 2]
    )";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin("Evaluate[Unevaluated[x_]]", "Unevaluated[x]");
		builtin("Evaluate[x___]", "x");
	}
};

void Builtins::Evaluation::initialize() {
	add<Hold>();
	add<HoldComplete>();
    add<HoldForm>();
	add<EvaluateBuiltin>();
}