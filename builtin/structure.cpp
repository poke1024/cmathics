#include "structure.h"

class Head : public Builtin {
public:
    static constexpr const char *name = "Head";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        builtin(&Head::apply);
    }

    inline BaseExpressionRef apply(
        BaseExpressionPtr expr,
        const Evaluation &evaluation) {

        return BaseExpressionRef(expr->head(evaluation));
    }
};

class PatternsOrderedQ : public Builtin {
public:
	static constexpr const char *name = "PatternsOrderedQ";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'PatternsOrderedQ[$patt1$, $patt2$]'
        <dd>returns 'True' if pattern $patt1$ would be applied before
        $patt2$ according to canonical pattern ordering.
    </dl>

    >> PatternsOrderedQ[x__, x_]
     = False
    >> PatternsOrderedQ[x_, x__]
     = True
    >> PatternsOrderedQ[b, a]
     = True
	)";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&PatternsOrderedQ::apply);
	}

	inline BaseExpressionRef apply(
		BaseExpressionPtr p1,
		BaseExpressionPtr p2,
		const Evaluation &evaluation) {

		auto x1 = p1->pattern_key();
		auto x2 = p2->pattern_key();

		return evaluation.Boolean(
			p1->pattern_key().compare(p2->pattern_key()) <= 0);
	}
};

class OrderedQ : public Builtin {
public:
	static constexpr const char *name = "OrderedQ";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'OrderedQ[$a$, $b$]'
        <dd>is 'True' if $a$ sorts before $b$ according to canonical
        ordering.
    </dl>

    >> OrderedQ[a, b]
     = True
    >> OrderedQ[b, a]
     = False
	)";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&OrderedQ::apply);
	}

	inline BaseExpressionRef apply(
		BaseExpressionPtr p1,
		BaseExpressionPtr p2,
		const Evaluation &evaluation) {

		return evaluation.Boolean(
			p1->sort_key().compare(p2->sort_key()) <= 0);
	}
};

void Builtins::Structure::initialize() {
    add<Head>();
	add<PatternsOrderedQ>();
	add<OrderedQ>();
}
