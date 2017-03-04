#include "comparison.h"
#include "../arithmetic/binary.h"
#include <symengine/eval.h>
#include "arithmetic/compare.tcc"

template<bool Unequal>
class EqualComparison : public BinaryOperator<equal<Unequal>> {
public:
    using Base = BinaryOperator<equal<Unequal>>;

    EqualComparison(const Definitions &definitions) : Base(definitions) {
        Base::template init<Symbol, Symbol>([] (
			const BaseExpressionPtr a,
			const BaseExpressionPtr b,
			const Evaluation &evaluation) -> tribool {

                if (a == b) {
                    return !Unequal;
                } else {
                    return undecided;
                }
            }
        );

        Base::template clear<MachineReal, MachineReal>();
        Base::template clear<MachineReal, BigReal>();
        Base::template clear<BigReal, MachineReal>();
        Base::template clear<BigReal, BigReal>();
    }
};

template<int N>
class ConstantTrueRule :
	public ExactlyNRule<N>,
	public ExtendedHeapObject<ConstantTrueRule<N>> {

public:
	ConstantTrueRule(const SymbolRef &head, const Evaluation &evaluation) :
		ExactlyNRule<N>(head, evaluation) {
	}

	virtual optional<BaseExpressionRef> try_apply(
		const Expression *expr,
		const Evaluation &evaluation) const {

		return BaseExpressionRef(evaluation.True);
	}
};

template<typename Operator>
class CompareNRule :
    public AtLeastNRule<3>,
    public ExtendedHeapObject<CompareNRule<Operator>> {

private:
    const SymbolRef m_head;
    const Operator m_operator;

public:
    inline CompareNRule(const SymbolRef &head, const Evaluation &evaluation) :
        AtLeastNRule<3>(head, evaluation), m_head(head), m_operator(evaluation.definitions) {
    }

    virtual optional<BaseExpressionRef> try_apply(
        const Expression *expr,
        const Evaluation &evaluation) const {

        return expr->with_slice([this, &evaluation] (const auto &slice) -> BaseExpressionRef {
            const size_t n = slice.size();

            for (size_t i = 0; i < n - 1; i++) {
                switch(m_operator(slice[i].get(), slice[i + 1].get(), evaluation)) {
	                case true:
		                break;
	                case false:
		                return BaseExpressionRef(evaluation.False);
	                case undecided:
		                return BaseExpressionRef();
                }
            }

            return BaseExpressionRef(evaluation.True);
        });
    }
};

class CompareUnequalNRule : public AtLeastNRule<3>, public ExtendedHeapObject<CompareUnequalNRule> {
private:
    const SymbolRef m_head;
    const EqualComparison<false> m_is_equal;

public:
    inline CompareUnequalNRule(const SymbolRef &head, const Evaluation &evaluation) :
        AtLeastNRule<3>(head, evaluation), m_head(head), m_is_equal(evaluation.definitions) {
    }

    virtual optional<BaseExpressionRef> try_apply(
        const Expression *expr,
        const Evaluation &evaluation) const {

        return expr->with_slice([this, &evaluation] (const auto &slice) -> BaseExpressionRef {
            const size_t n = slice.size();

            for (size_t i = 0; i < n; i++) {
                const BaseExpressionRef leaf = slice[i];

                for (size_t j = i + 1; j < n; j++) {
                    switch(m_is_equal(leaf.get(), slice[j].get(), evaluation)) {
	                    case true:
		                    return BaseExpressionRef(evaluation.False);
	                    case false:
		                    break;
	                    case undecided:
		                    return BaseExpressionRef();
                    }
                }
            }

            return BaseExpressionRef(evaluation.True);
        });
    }
};

template<typename T>
class ComparisonBuiltin : public BinaryOperatorBuiltin {
public:
    using BinaryOperatorBuiltin::BinaryOperatorBuiltin;

    void build(Runtime &runtime) {
        add_binary_operator_formats();
        builtin<ConstantTrueRule<0>>();
        builtin<ConstantTrueRule<1>>();
        builtin<BinaryComparisonRule<T>>();
        builtin<CompareNRule<T>>();
    }
};

class Equal : public BinaryOperatorBuiltin {
public:
    static constexpr const char *name = "Equal";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'Equal[$x$, $y$]'
    <dt>'$x$ == $y$'
        <dd>yields 'True' if $x$ and $y$ are known to be equal, or
        'False' if $x$ and $y$ are known to be unequal.
    <dt>'$lhs$ == $rhs$'
        <dd>represents the equation $lhs$ = $rhs$.
    </dl>

    >> a==a
     = True
    >> a==b
     = a == b
    >> 1==1.
     = True

    Lists are compared based on their elements:
    >> {{1}, {2}} == {{1}, {2}}
     = True
    >> {1, 2} == {1, 2, 3}
     = False

    Real values are considered equal if they only differ in their last digits:
    >> 0.739085133215160642 == 0.739085133215160641
     = True
    >> 0.73908513321516064200000000 == 0.73908513321516064100000000
     = False

    ## TODO Needs power precision tracking
    ## >> 0.1 ^ 10000 == 0.1 ^ 10000 + 0.1 ^ 10012
    ##  = False
    ## >> 0.1 ^ 10000 == 0.1 ^ 10000 + 0.1 ^ 10013
    ##  = True

    >> 0.1111111111111111 ==  0.1111111111111126
     = True
    >> 0.1111111111111111 ==  0.1111111111111127
     = False

    ## TODO needs better precision tracking
    ## #> 2^^1.000000000000000000000000000000000000000000000000000000000000 ==  2^^1.000000000000000000000000000000000000000000000000000001111111
    ##  = True
    ## #> 2^^1.000000000000000000000000000000000000000000000000000000000000 ==  2^^1.000000000000000000000000000000000000000000000000000010000000
    ##  = False

    Comparisons are done using the lower precision:
    >> N[E, 100] == N[E, 150]
     = True

    Symbolic constants are compared numerically:
    >> E > 1
     = True
    #> Pi == 3.14
     = False

    >> Pi ^ E == E ^ Pi
     = False

    >> N[E, 3] == N[E]
     = True

    >> {1, 2, 3} < {1, 2, 3}
     = {1, 2, 3} < {1, 2, 3}

    #> E == N[E]
     = True

    ## Issue260
    >> {Equal[Equal[0, 0], True], Equal[0, 0] == True}
     = {True, True}
    #> {Mod[6, 2] == 0, Mod[6, 4] == 0, (Mod[6, 2] == 0) == (Mod[6, 4] == 0), (Mod[6, 2] == 0) != (Mod[6, 4] == 0)}
     = {True, False, False, True}

    >> a == a == a
     = True

    >> {Equal[], Equal[x], Equal[1]}
     = {True, True, True}
    )";

    virtual const char *operator_name() const {
        return "==";
    }

    virtual int precedence() const {
        return 290;
    }

    virtual const char *grouping() const {
        return "NonAssociative";
    }

public:
    using BinaryOperatorBuiltin::BinaryOperatorBuiltin;

    void build(Runtime &runtime) {
        add_binary_operator_formats();
        builtin<ConstantTrueRule<0>>();
        builtin<ConstantTrueRule<1>>();
        builtin<BinaryComparisonRule<EqualComparison<false>>>();
        builtin<CompareNRule<EqualComparison<false>>>();
    }
};

class Unequal : public BinaryOperatorBuiltin {
public:
    static constexpr const char *name = "Unequal";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'Unequal[$x$, $y$]'
    <dt>'$x$ != $y$'
        <dd>yields 'False' if $x$ and $y$ are known to be equal, or
        'True' if $x$ and $y$ are known to be unequal.
    <dt>'$lhs$ == $rhs$'
        <dd>represents the inequality $lhs$ ≠ $rhs$.
    </dl>

    >> 1 != 1.
     = False

    Lists are compared based on their elements:
    >> {1} != {2}
     = True
    >> {1, 2} != {1, 2}
     = False
    >> {a} != {a}
     = False
    >> "a" != "b"
     = True
    >> "a" != "a"
     = False

    #> Pi != N[Pi]
     = False

    #> a_ != b_
     = a_ != b_

    >> a != a != a
     = False
    >> "abc" != "def" != "abc"
     = False

    ## Reproduce strange MMA behaviour
    >> a != a != b
     = False
    >> a != b != a
     = a != b != a

    >> {Unequal[], Unequal[x], Unequal[1]}
     = {True, True, True}
    )";

    virtual const char *operator_name() const {
        return "!=";
    }

    virtual int precedence() const {
        return 290;
    }

    virtual const char *grouping() const {
        return "NonAssociative";
    }

public:
    using BinaryOperatorBuiltin::BinaryOperatorBuiltin;

    void build(Runtime &runtime) {
        add_binary_operator_formats();
        builtin<ConstantTrueRule<0>>();
        builtin<ConstantTrueRule<1>>();
        builtin<BinaryComparisonRule<EqualComparison<true>>>();
        builtin<CompareUnequalNRule>();
    }
};

template<typename Operator>
class InequalityOperator : public ComparisonBuiltin<BinaryOperator<Operator>> {
public:
    virtual int precedence() const {
        return 290;
    }

    virtual const char *grouping() const {
        return "NonAssociative";
    }

    using ComparisonBuiltin<BinaryOperator<Operator>>::ComparisonBuiltin;
};

class Less : public InequalityOperator<less> {
public:
    static constexpr const char *name = "Less";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'Less[$x$, $y$]'
    <dt>'$x$ < $y$'
        <dd>yields 'True' if $x$ is known to be less than $y$.
    <dt>'$lhs$ < $rhs$'
        <dd>represents the inequality $lhs$ < $rhs$.
    </dl>

    >> {Less[], Less[x], Less[1]}
     = {True, True, True}
    )";

    virtual const char *operator_name() const {
        return "<";
    }

public:
    using InequalityOperator<less>::InequalityOperator;
};

class LessEqual : public InequalityOperator<less_equal> {
public:
    static constexpr const char *name = "LessEqual";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'LessEqual[$x$, $y$]'
    <dt>'$x$ <= $y$'
        <dd>yields 'True' if $x$ is known to be less than or equal to $y$.
    <dt>'$lhs$ <= $rhs$'
        <dd>represents the inequality $lhs$ ≤ $rhs$.
    </dl>
    )";

    virtual const char *operator_name() const {
        return "<=";
    }

public:
    using InequalityOperator<less_equal>::InequalityOperator;
};

class Greater : public InequalityOperator<greater> {
public:
    static constexpr const char *name = "Greater";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'Greater[$x$, $y$]'
    <dt>'$x$ > $y$'
        <dd>yields 'True' if $x$ is known to be greater than $y$.
    <dt>'$lhs$ > $rhs$'
        <dd>represents the inequality $lhs$ > $rhs$.
    </dl>
    >> a > b > c //FullForm
     = Greater[a, b, c]
    >> Greater[3, 2, 1]
     = True
    )";

    virtual const char *operator_name() const {
        return ">";
    }

public:
    using InequalityOperator<greater>::InequalityOperator;
};

class GreaterEqual : public InequalityOperator<greater_equal> {
public:
    static constexpr const char *name = "GreaterEqual";

    static constexpr const char *docs = R"(
    <dl>
    <dt>'GreaterEqual[$x$, $y$]'
    <dt>'$x$ >= $y$'
        <dd>yields 'True' if $x$ is known to be greater than or equal
        to $y$.
    <dt>'$lhs$ >= $rhs$'
        <dd>represents the inequality $lhs$ ≥ $rhs$.
    </dl>
    )";

    virtual const char *operator_name() const {
        return ">=";
    }

public:
    using InequalityOperator<greater_equal>::InequalityOperator;
};

class Positive : public Builtin {
public:
	static constexpr const char *name = "Positive";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Positive[$x$]'
        <dd>returns 'True' if $x$ is a positive real number.
    </dl>

    >> Positive[1]
     = True

    'Positive' returns 'False' if $x$ is zero or a complex number:
    >> Positive[0]
     = False
    >> Positive[1 + 2 I]
     = False

    #> Positive[Pi]
     = True
    #> Positive[x]
     = Positive[x]
    #> Positive[Sin[{11, 14}]]
     = {False, True}
    )";

	static constexpr auto attributes = Attributes::Listable;

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin("Positive[x_?NumericQ]", "If[x > 0, True, False, False]");
	}
};

class Negative : public Builtin {
public:
	static constexpr const char *name = "Negative";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Negative[$x$]'
        <dd>returns 'True' if $x$ is a negative real number.
    </dl>
    >> Negative[0]
     = False
    >> Negative[-3]
     = True
    >> Negative[10/7]
     = False
    >> Negative[1+2I]
     = False
    >> Negative[a + b]
     = Negative[a + b]
    #> Negative[-E]
     = True
    #> Negative[Sin[{11, 14}]]
     = {True, False}
    )";

	static constexpr auto attributes = Attributes::Listable;

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin("Negative[x_?NumericQ]", "If[x < 0, True, False, False]");
	}
};

class NonPositive : public Builtin {
public:
	static constexpr const char *name = "NonPositive";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'NonPositive[$x$]'
        <dd>returns 'True' if $x$ is a positive real number or zero.
    </dl>

    >> {Negative[0], NonPositive[0]}
     = {False, True}
    )";

	static constexpr auto attributes = Attributes::Listable;

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin("NonPositive[x_?NumericQ]", "If[x <= 0, True, False, False]");
	}
};

class NonNegative : public Builtin {
public:
	static constexpr const char *name = "NonNegative";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'NonNegative[$x$]'
        <dd>returns 'True' if $x$ is a positive real number or zero.
    </dl>

    >> {Positive[0], NonNegative[0]}
     = {False, True}
    )";

	static constexpr auto attributes = Attributes::Listable;

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin("NonNegative[x_?NumericQ]", "If[x >= 0, True, False, False]");
	}
};

void Builtins::Comparison::initialize() {
    add<Equal>();
    add<Unequal>();
    add<Less>();
    add<LessEqual>();
    add<Greater>();
    add<GreaterEqual>();
	add<Positive>();
	add<Negative>();
	add<NonPositive>();
	add<NonNegative>();
}
