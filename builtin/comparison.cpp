#include "comparison.h"
#include "../arithmetic/binary.h"
#include <symengine/eval.h>

template<int N>
class ConstantTrueRule :
    public ExactlyNRule<N>,
    public ExtendedHeapObject<ConstantTrueRule<N>> {

public:
    ConstantTrueRule(const SymbolRef &head, const Definitions &definitions) :
        ExactlyNRule<N>(head, definitions) {
    }

    virtual optional<BaseExpressionRef> try_apply(
         const Expression *expr,
         const Evaluation &evaluation) const {

        return BaseExpressionRef(evaluation.True);
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

template<bool Less, bool Equal, bool Greater>
struct inequality {
	static inline BaseExpressionRef fallback(
		const ExpressionPtr expr,
		const Evaluation &evaluation) {

		const auto * const leaves = expr->n_leaves<2>();

		const BaseExpression * const a = leaves[0].get();
		const BaseExpression * const b = leaves[1].get();

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

				try {
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
				} catch (const SymEngine::SymEngineException &e) {
					evaluation.sym_engine_exception(e);
					return BaseExpressionRef();
				}
			}
		}

		return BaseExpressionRef();
	}
};

struct less : public inequality<true, false, false> {
    template<typename U, typename V>
    static inline BaseExpressionRef function(const U &u, const V &v, const Evaluation &evaluation) {
        return evaluation.Boolean(Comparison<U, V>::compare(u, v, [] (const auto &x, const auto &y) {
            return x < y;
        }));
    }
};

struct less_equal : public inequality<true, true, false> {
    template<typename U, typename V>
    static inline BaseExpressionRef function(const U &u, const V &v, const Evaluation &evaluation) {
        return evaluation.Boolean(Comparison<U, V>::compare(u, v, [] (const auto &x, const auto &y) {
            return x <= y;
        }));
    }
};

struct greater : public inequality<false, false, true> {
    template<typename U, typename V>
    static inline BaseExpressionRef function(const U &u, const V &v, const Evaluation &evaluation) {
        return evaluation.Boolean(Comparison<U, V>::compare(u, v, [] (const auto &x, const auto &y) {
            return x > y;
        }));
    }
};

struct greater_equal : public inequality<false, true, true> {
    template<typename U, typename V>
    static inline BaseExpressionRef function(const U &u, const V &v, const Evaluation &evaluation) {
        return evaluation.Boolean(Comparison<U, V>::compare(u, v, [] (const auto &x, const auto &y) {
            return x >= y;
        }));
    }
};

template<bool Negate>
inline bool negate(bool f) {
    return Negate ? !f : f;
}

template<bool Unequal>
struct equal {
    template<typename U, typename V>
    static inline BaseExpressionRef function(const U &u, const V &v, const Evaluation &evaluation) {
        return evaluation.Boolean(Comparison<U, V>::compare(u, v, [] (const auto &x, const auto &y) {
            return negate<Unequal>(x == y);
        }));
    }

	static inline BaseExpressionRef fallback(
		const ExpressionPtr expr,
		const Evaluation &evaluation) {

		const auto * const leaves = expr->n_leaves<2>();

		const BaseExpression * const a = leaves[0].get();
		const BaseExpression * const b = leaves[1].get();

		switch (a->equals(*b)) {
			case true:
				return evaluation.Boolean(negate<Unequal>(true));
			case false:
				return evaluation.Boolean(negate<Unequal>(false));
			case undecided:
				return BaseExpressionRef();
			default:
				throw std::runtime_error("illegal equals result");
		}
	}
};

template<bool Unequal>
class EqualComparison : public BinaryArithmetic<equal<Unequal>> {
public:
    using Base = BinaryArithmetic<equal<Unequal>>;

    EqualComparison(const Definitions &definitions) : Base(definitions) {
        Base::template init<Symbol, Symbol>([] (
			const ExpressionPtr expr,
		    const Evaluation& evaluation) {

		        const auto * const leaves = expr->n_leaves<2>();

		        const BaseExpression * const a = leaves[0].get();
		        const BaseExpression * const b = leaves[1].get();

                if (a == b) {
                    return BaseExpressionRef(evaluation.Boolean(!Unequal));
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

template<typename Operator>
class CompareNRule :
    public AtLeastNRule<3>,
    public ExtendedHeapObject<CompareNRule<Operator>> {

private:
    const SymbolRef m_head;
    const Operator m_operator;

public:
    inline CompareNRule(const SymbolRef &head, const Definitions &definitions) :
        AtLeastNRule<3>(head, definitions), m_head(head), m_operator(definitions) {
    }

    virtual optional<BaseExpressionRef> try_apply(
        const Expression *expr,
        const Evaluation &evaluation) const {

        return expr->with_slice([this, &evaluation] (const auto &slice) {
            const size_t n = slice.size();

            for (size_t i = 0; i < n - 1; i++) {
	            const ExpressionRef expr = expression(
		            m_head, slice[i], slice[i + 1]);

                const BaseExpressionRef result = m_operator(
                    evaluation.definitions,
                    expr.get(),
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

class CompareUnequalNRule : public AtLeastNRule<3>, public ExtendedHeapObject<CompareUnequalNRule> {
private:
    const SymbolRef m_head;
    const EqualComparison<false> m_is_equal;

public:
    inline CompareUnequalNRule(const SymbolRef &head, const Definitions &definitions) :
        AtLeastNRule<3>(head, definitions), m_head(head), m_is_equal(definitions) {
    }

    virtual optional<BaseExpressionRef> try_apply(
        const Expression *expr,
        const Evaluation &evaluation) const {

        return expr->with_slice([this, &evaluation] (const auto &slice) {
            const size_t n = slice.size();

            for (size_t i = 0; i < n; i++) {
                const BaseExpressionRef leaf = slice[i];

                for (size_t j = i + 1; j < n; j++) {
	                const ExpressionRef expr =
		                expression(m_head, leaf, slice[j]);

                    const BaseExpressionRef is_equal = m_is_equal(
                        evaluation.definitions,
                        expr.get(),
                        evaluation);

                    if (!is_equal) { // undefined?
                        return BaseExpressionRef();
                    }

                    if (is_equal->is_true()) {
                        return BaseExpressionRef(evaluation.False);
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
        builtin<BinaryOperatorRule<T>>();
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
        builtin<BinaryOperatorRule<EqualComparison<false>>>();
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
        builtin<BinaryOperatorRule<EqualComparison<true>>>();
        builtin<CompareUnequalNRule>();
    }
};

template<typename Operator>
class InequalityOperator : public ComparisonBuiltin<BinaryArithmetic<Operator>> {
public:
    virtual int precedence() const {
        return 290;
    }

    virtual const char *grouping() const {
        return "NonAssociative";
    }

    using ComparisonBuiltin<BinaryArithmetic<Operator>>::ComparisonBuiltin;
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

void Builtins::Comparison::initialize() {
    add<Equal>();
    add<Unequal>();
    add<Less>();
    add<LessEqual>();
    add<Greater>();
    add<GreaterEqual>();
}
