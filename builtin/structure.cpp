#include "structure.h"

class Sort : public Builtin {
public:
	static constexpr const char *name = "Sort";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Sort[$list$]'
    <dd>sorts $list$ (or the leaves of any other expression) according to canonical ordering.
    <dt>'Sort[$list$, $p$]'
    <dd>sorts using $p$ to determine the order of two elements.
    </dl>

    >> Sort[{4, 1.0, a, 3+I}]
     = {1., 3 + I, 4, a}

    Sort uses 'OrderedQ' to determine ordering by default.
    You can sort patterns according to their precedence using 'PatternsOrderedQ':
    >> Sort[{items___, item_, OptionsPattern[], item_symbol, item_?test}, PatternsOrderedQ]
     = {item_symbol, item_ ? test, item_, items___, OptionsPattern[]}

    When sorting patterns, values of atoms do not matter:
    >> Sort[{a, b/;t}, PatternsOrderedQ]
     = {b /; t, a}
    >> Sort[{2+c_, 1+b__}, PatternsOrderedQ]
     = {2 + c_, 1 + b__}
    >> Sort[{x_ + n_*y_, x_ + y_}, PatternsOrderedQ]
     = {x_ + n_ y_, x_ + y_}

    >> Sort[{x_, y_}, PatternsOrderedQ]
     = {x_, y_}
	)";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&Sort::apply);
		builtin(&Sort::apply_predicate);
	}

	inline BaseExpressionRef apply(
		BaseExpressionPtr expr,
		const Evaluation &evaluation) {

		if (!expr->is_expression()) {
			evaluation.message(m_symbol, "normal");
			return BaseExpressionRef();
		}

		return expr->as_expression()->with_slice([expr, &evaluation] (const auto &slice) {
			return ::sorted(slice, expr->as_expression()->head(), evaluation);
		});
	}

	inline BaseExpressionRef apply_predicate(
		BaseExpressionPtr expr,
		BaseExpressionPtr predicate,
		const Evaluation &evaluation) {

		if (!expr->is_expression()) {
			evaluation.message(m_symbol, "normal");
			return BaseExpressionRef();
		}

		return expr->as_expression()->with_slice([expr, predicate, &evaluation] (const auto &slice) {
			const size_t n = slice.size();

			TemporaryRefVector refs;
			refs.reserve(n);
			for (size_t i = 0; i < n; i++) {
				refs.push_back(slice[i]);
			}

			std::sort(refs.begin(), refs.end(), [predicate, &evaluation] (const auto &i, const auto &j) {
				return expression(predicate, i, j)->evaluate_or_copy(evaluation)->is_true();
			});

			return refs.to_expression(expr->as_expression()->head());
		});
	}
};

class Order : public Builtin {
public:
	static constexpr const char *name = "Order";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Order[$x$, $y$]'
        <dd>returns a number indicating the canonical ordering of $x$ and $y$. 1 indicates that $x$ is before $y$,
        -1 that $y$ is before $x$. 0 indicates that there is no specific ordering. Uses the same order as 'Sort'.
    </dl>

    >> Order[7, 11]
     = 1

    >> Order[100, 10]
     = -1

    >> Order[x, z]
     = 1

    >> Order[x, x]
     = 0
	)";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&Order::apply);
	}

	inline BaseExpressionRef apply(
		BaseExpressionPtr p1,
		BaseExpressionPtr p2,
		const Evaluation &evaluation) {

		SortKey k1, k2;

		p1->sort_key(k1, evaluation);
		p2->sort_key(k2, evaluation);

		const auto x = k1.compare(k2, evaluation);

		if (x < 0) {
			return evaluation.definitions.one;
		} else if (x > 0) {
			return evaluation.definitions.minus_one;
		} else {
			return evaluation.zero;
		}
	}
};

class Head : public Builtin {
public:
    static constexpr const char *name = "Head";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Head[$expr$]'
        <dd>returns the head of the expression or atom $expr$.
    </dl>

    >> Head[a * b]
     = Times
    >> Head[6]
     = Integer
    >> Head[x]
     = Symbol
	)";

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

		SortKey k1, k2;

		p1->pattern_key(k1, evaluation);
		p2->pattern_key(k2, evaluation);

		return evaluation.Boolean(k1.compare(k2, evaluation) <= 0);
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

		SortKey k1;
		SortKey k2;

		p1->sort_key(k1, evaluation);
		p2->sort_key(k2, evaluation);

		return evaluation.Boolean(k1.compare(k2, evaluation) <= 0);
	}
};

void Builtins::Structure::initialize() {
	add<Sort>();
	add<Order>();
    add<Head>();
	add<PatternsOrderedQ>();
	add<OrderedQ>();
}
