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

struct less {
    template<typename U, typename V>
    static optional<bool> calculate(const U &u, const V &v) {
        return Comparison<U, V>::compare(u, v, [] (const auto &x, const auto &y) {
            return x < y;
        });
    }
};

struct less_equal {
    template<typename U, typename V>
    static optional<bool> calculate(const U &u, const V &v) {
        return Comparison<U, V>::compare(u, v, [] (const auto &x, const auto &y) {
            return x <= y;
        });
    }
};

template<bool Less, bool Equal, bool Greater>
struct inequality_fallback {
public:
	inline BaseExpressionRef operator()(
        const BaseExpressionPtr head,
        const BaseExpressionPtr a,
        const BaseExpressionPtr b,
        const Evaluation &evaluation) const {

		const SymbolicFormRef sym_a = symbolic_form(a, evaluation);
		if (!sym_a->is_none()) {
			const SymbolicFormRef sym_b = symbolic_form(b, evaluation);
			if (!sym_b->is_none()) {
                if (sym_a->get()->__eq__(*sym_b->get())) {
                    return evaluation.Boolean(Equal);
                }

                const Precision pa = precision(a);
                const Precision pb = precision(b);

                const Precision p = std::max(pa, pb);
                const Precision p0 = std::min(pa, pb);

                unsigned long prec = p.bits;
				const auto diff = SymEngine::sub(sym_a->get(), sym_b->get());

                while (true) {
                    const auto ndiff = SymEngine::evalf(*diff, prec, true);

                    if (ndiff->is_negative()) {
                        return evaluation.Boolean(Less);
                    } else if (ndiff->is_positive()) {
                        return evaluation.Boolean(Greater);
                    } else {
                        if (p0.is_none()) {
                            // we're probably dealing with an irrational as
                            // in x < Pi. increase precision, try again.
                            assert(!__builtin_umull_overflow(prec, 2, &prec));
                        } else {
                            return evaluation.Boolean(Equal);
                        }
                    }
                }
			}
		}

		return BaseExpressionRef();
	}
};

struct greater {
    template<typename U, typename V>
    static optional<bool> calculate(const U &u, const V &v) {
        return Comparison<U, V>::compare(u, v, [] (const auto &x, const auto &y) {
            return x > y;
        });
    }
};

struct greater_equal {
    template<typename U, typename V>
    static optional<bool> calculate(const U &u, const V &v) {
        return Comparison<U, V>::compare(u, v, [] (const auto &x, const auto &y) {
            return x >= y;
        });
    }
};

template<typename Operator>
class CompareNRule : public AtLeastNRule<3> {
private:
    const SymbolRef m_head;
    const Operator m_operator;

public:
    inline CompareNRule(const SymbolRef &head, const Definitions &definitions) :
        AtLeastNRule<3>(head, definitions), m_head(head), m_operator(definitions) {
    }

    virtual BaseExpressionRef try_apply(
        const Expression *expr, const Evaluation &evaluation) const {

        return expr->with_slice([this, &evaluation] (const auto &slice) {
            const size_t n = slice.size();

            for (size_t i = 0; i < n - 1; i++) {
                const BaseExpressionRef result = m_operator(
                    evaluation.definitions,
                    expression(m_head, slice[i], slice[i + 1]).get(),
                    evaluation);

                if (!result) { // undefined?
                    return BaseExpressionRef();
                }

                if (!result->is_true()) {
                    return BaseExpressionRef(evaluation.False);
                }
            }

            return BaseExpressionRef(evaluation.True);
        });
    }
};

template<bool Negate>
inline bool negate(bool f) {
    return Negate ? !f : f;
}

template<bool Unequal>
struct equal {
    template<typename U, typename V>
    static optional<bool> calculate(const U &u, const V &v) {
        return Comparison<U, V>::compare(u, v, [] (const auto &x, const auto &y) {
            return negate<Unequal>(x == y);
        });
    }
};

template<bool Unequal>
class equal_fallback {
public:
    inline BaseExpressionRef operator()(
        const BaseExpressionPtr head,
        const BaseExpressionPtr a,
        const BaseExpressionPtr b,
        const Evaluation &evaluation) const {

        return evaluation.Boolean(negate<Unequal>(a->equals(*b)));
    }
};

template<bool Unequal>
class EqualComparison : public BinaryArithmetic<equal<Unequal>, equal_fallback<Unequal>> {
public:
    using Base = BinaryArithmetic<equal<Unequal>, equal_fallback<Unequal>>;

    EqualComparison(const Definitions &definitions) : Base(definitions) {
        Base::template init<Symbol, Symbol>(
            [] (const BaseExpression *a, const BaseExpression *b) -> optional<bool> {
                if (a == b) {
                    return Unequal ? false : true;
                } else {
                    return optional<bool>();
                }
            }
        );

        Base::template clear<MachineReal, MachineReal>();
        Base::template clear<MachineReal, BigReal>();
        Base::template clear<BigReal, MachineReal>();
        Base::template clear<BigReal, BigReal>();
    }
};

template<typename T>
class ComparisonBuiltin : public Builtin {
public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        rule<ConstantTrueRule<0>>();
        rule<ConstantTrueRule<1>>();
        rule<BinaryOperatorRule<T>>();
        rule<CompareNRule<T>>();
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
    >> Pi == 3.14
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

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        rule<ConstantTrueRule<0>>();
        rule<ConstantTrueRule<1>>();
        rule<BinaryOperatorRule<EqualComparison<false>>>();
        rule<CompareNRule<EqualComparison<false>>>();
    }
};

class Unequal : public Builtin {
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
    #> "abc" != "def" != "abc"
     = False

    ## Reproduce strange MMA behaviour
    >> a != a != b
     = False
    >> a != b != a
     = a != b != a

    >> {Unequal[], Unequal[x], Unequal[1]}
     = {True, True, True}
    )";

public:
    using Builtin::Builtin;

    void build(Runtime &runtime) {
        rule<ConstantTrueRule<0>>();
        rule<ConstantTrueRule<1>>();
        rule<BinaryOperatorRule<EqualComparison<true>>>();
        rule<CompareNRule<EqualComparison<true>>>();
    }
};

using LessBase = ComparisonBuiltin<BinaryArithmetic<less, inequality_fallback<true, false, false>>>;

class Less : public LessBase {
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

public:
    using LessBase::LessBase;
};

using LessEqualBase = ComparisonBuiltin<BinaryArithmetic<less_equal, inequality_fallback<true, true, false>>>;

class LessEqual : public LessEqualBase {
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

public:
    using LessEqualBase::LessEqualBase;
};

using GreaterBase = ComparisonBuiltin<BinaryArithmetic<greater, inequality_fallback<false, false, true>>>;

class Greater : public GreaterBase {
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

public:
    using GreaterBase::GreaterBase;
};

using GreaterEqualBase = ComparisonBuiltin<BinaryArithmetic<greater_equal, inequality_fallback<false, true, true>>>;

class GreaterEqual : public GreaterEqualBase {
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

public:
    using GreaterEqualBase::GreaterEqualBase;
};

void Builtins::Comparison::initialize() {
    add<Equal>();
    add<Unequal>();
    add<Less>();
    add<LessEqual>();
    add<Greater>();
    add<GreaterEqual>();
}
