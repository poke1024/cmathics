#include "comparison.h"
#include "../arithmetic/binary.h"
#include <symengine/eval.h>

template<int N>
class ConstantTrueRule : public ExactlyNRule<N> {
public:
    ConstantTrueRule(const SymbolRef &head, const Definitions &definitions) :
        ExactlyNRule<N>(head, definitions) {
    }

    virtual BaseExpressionRef try_apply(
         const Expression *expr, const Evaluation &evaluation) const {

        return evaluation.True;
    }
};

template<typename U, typename V>
struct Comparison {
    template<typename F>
    inline static bool compare(const U &u, const V &v, const F &f) {
        return f(u.value, v.value);
    }
};

template<>
struct Comparison<BigInteger, MachineInteger> {
    template<typename F>
    inline static bool compare(const BigInteger &u, const MachineInteger &v, const F &f) {
        return f(u.value, long(v.value));
    }
};

template<>
struct Comparison<MachineInteger, BigInteger> {
    template<typename F>
    inline static bool compare(const MachineInteger &u, const BigInteger &v, const F &f) {
        return f(long(u.value), v.value);
    }
};

template<>
struct Comparison<BigRational, MachineInteger> {
    template<typename F>
    inline static bool compare(const BigRational &u, const MachineInteger &v, const F &f) {
        return f(u.value, long(v.value));
    }
};

template<>
struct Comparison<MachineInteger, BigRational> {
    template<typename F>
    inline static bool compare(const MachineInteger &u, const BigRational &v, const F &f) {
        return f(long(u.value), v.value);
    }
};

template<>
struct Comparison<MachineInteger, BigReal> {
    template<typename F>
    inline static bool compare(const MachineInteger &u, const BigReal &v, const F &f) {
        return f(Numeric::R(u.value), Numeric::R(v.value));
    }
};

template<>
struct Comparison<BigReal, MachineInteger> {
    template<typename F>
    inline static bool compare(const BigReal &u, const MachineInteger &v, const F &f) {
        return f(Numeric::R(u.value), Numeric::R(v.value));
    }
};

template<>
struct Comparison<MachineReal, BigReal> {
    template<typename F>
    inline static bool compare(const MachineReal &u, const BigReal &v, const F &f) {
        return f(Numeric::R(u.value), Numeric::R(v.value));
    }
};

template<>
struct Comparison<BigReal, MachineReal> {
    template<typename F>
    inline static bool compare(const BigReal &u, const MachineReal &v, const F &f) {
        return f(Numeric::R(u.value), Numeric::R(v.value));
    }
};

template<>
struct Comparison<BigInteger, BigReal> {
    template<typename F>
    inline static bool compare(const BigInteger &u, const BigReal &v, const F &f) {
        return f(Numeric::R(u.value), Numeric::R(v.value));
    }
};

template<>
struct Comparison<BigReal, BigInteger> {
    template<typename F>
    inline static bool compare(const BigReal &u, const BigInteger &v, const F &f) {
        return f(Numeric::R(u.value), Numeric::R(v.value));
    }
};

template<>
struct Comparison<BigRational, BigReal> {
    template<typename F>
    inline static bool compare(const BigRational &u, const BigReal &v, const F &f) {
        return f(Numeric::R(u.value, v.prec), Numeric::R(v.value));
    }
};

template<>
struct Comparison<BigReal, BigRational> {
    template<typename F>
    inline static bool compare(const BigReal &u, const BigRational &v, const F &f) {
        return f(Numeric::R(u.value), Numeric::R(v.value, u.prec));
    }
};

struct equal {
    template<typename U, typename V>
    static bool calculate(const U &u, const V &v) {
        return Comparison<U, V>::compare(u, v, [] (const auto &x, const auto &y) {
            return x == y;
        });
    }
};

struct less {
    template<typename U, typename V>
    static bool calculate(const U &u, const V &v) {
        return Comparison<U, V>::compare(u, v, [] (const auto &x, const auto &y) {
            return x < y;
        });
    }
};

struct less_equal {
    template<typename U, typename V>
    static bool calculate(const U &u, const V &v) {
        return Comparison<U, V>::compare(u, v, [] (const auto &x, const auto &y) {
            return x <= y;
        });
    }
};

struct less_fallback {
public:
	inline BaseExpressionRef operator()(const Expression *expr, const Evaluation &evaluation) const {
		const BaseExpressionRef * const leaves = expr->static_leaves<2>();

		const BaseExpressionRef &a = leaves[0];
		const BaseExpressionRef &b = leaves[1];

		// FIXME perform symbolic comparisons, e.g. 5 < E
		const SymbolicFormRef sym_a = symbolic_form(a, evaluation);
		if (!sym_a->is_none()) {
			const SymbolicFormRef sym_b = symbolic_form(b, evaluation);
			if (!sym_b->is_none()) {
				const int prec = 53; // FIXME
				const auto diff = SymEngine::sub(sym_a->get(), sym_b->get());
				auto sign = SymEngine::evalf(*diff, prec, true);
				bool lt = sign->is_negative();
				return evaluation.Boolean(lt);
			}
		}

		return BaseExpressionRef();
	}
};

struct greater {
    template<typename U, typename V>
    static bool calculate(const U &u, const V &v) {
        return Comparison<U, V>::compare(u, v, [] (const auto &x, const auto &y) {
            return x > y;
        });
    }
};

struct greater_equal {
    template<typename U, typename V>
    static bool calculate(const U &u, const V &v) {
        return Comparison<U, V>::compare(u, v, [] (const auto &x, const auto &y) {
            return x >= y;
        });
    }
};

class equal_fallback {
public:
    inline BaseExpressionRef operator()(const Expression *expr, const Evaluation &evaluation) const {
        const BaseExpressionRef * const leaves = expr->static_leaves<2>();

        const BaseExpressionRef &a = leaves[0];
        const BaseExpressionRef &b = leaves[1];

        return evaluation.Boolean(a->equals(*b));
    }
};

class EqualComparison : public BinaryArithmetic<equal, equal_fallback> {
public:
    using Base = BinaryArithmetic<equal, equal_fallback>;

    EqualComparison(const Definitions &definitions) : Base(definitions) {
        const BaseExpressionRef True = definitions.symbols().True;

        Base::template init<Symbol, Symbol>(
            [True] (const BaseExpression *a, const BaseExpression *b) {
                if (a == b) {
                    return True;
                } else {
                    return BaseExpressionRef();
                }
            }
        );

        Base::template clear<MachineReal, MachineReal>();
        Base::template clear<MachineReal, BigReal>();
        Base::template clear<BigReal, MachineReal>();
        Base::template clear<BigReal, BigReal>();
    }
};

class Equal : public Builtin {
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

    #> 0.1111111111111111 ==  0.1111111111111126
     = True
    #> 0.1111111111111111 ==  0.1111111111111127
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
    >> Pi == 3.14
     = False

    #> Pi ^ E == E ^ Pi
     = False

    #> N[E, 3] == N[E]
     = True

    #> {1, 2, 3} < {1, 2, 3}
     = {1, 2, 3} < {1, 2, 3}

    #> E == N[E]
     = True

    ## Issue260
    #> {Equal[Equal[0, 0], True], Equal[0, 0] == True}
     = {True, True}
    #> {Mod[6, 2] == 0, Mod[6, 4] == 0, (Mod[6, 2] == 0) == (Mod[6, 4] == 0), (Mod[6, 2] == 0) != (Mod[6, 4] == 0)}
     = {True, False, False, True}

    #> a == a == a
     = True

    #> {Equal[], Equal[x], Equal[1]}
     = {True, True, True}
    )";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        rule<ConstantTrueRule<0>>();
        rule<ConstantTrueRule<1>>();
        rule<BinaryOperatorRule<EqualComparison>>();
    }
};

class Less : public Builtin {
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

    #> {Less[], Less[x], Less[1]}
     = {True, True, True}
    )";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        rule<ConstantTrueRule<0>>();
        rule<ConstantTrueRule<1>>();
        rule<BinaryOperatorRule<BinaryArithmetic<less, less_fallback>>>();
    }
};

constexpr auto LessEqual = NewRule<BinaryOperatorRule<BinaryArithmetic<less_equal>>>;
constexpr auto Greater = NewRule<BinaryOperatorRule<BinaryArithmetic<greater>>>;
constexpr auto GreaterEqual = NewRule<BinaryOperatorRule<BinaryArithmetic<greater_equal>>>;

void Builtins::Comparison::initialize() {
    add<Equal>();
    add<Less>();

    add("LessEqual",
        Attributes::None, {
                LessEqual
        });

    add("Greater",
        Attributes::None, {
                Greater
        });

    add("GreaterEqual",
        Attributes::None, {
                GreaterEqual
        });
}
