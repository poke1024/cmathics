// @formatter:off

#include "arithmetic.h"
#include "../core/definitions.h"
#include "../arithmetic/add.h"
#include "../arithmetic/mul.h"
#include "../arithmetic/binary.h"

static_assert(sizeof(machine_integer_t) == sizeof(long),
	"machine integer type must equivalent to long");

inline BaseExpressionRef operator+(const MachineInteger &x, const MachineInteger &y) {
	long r;
	if (!__builtin_saddl_overflow(x.value, y.value, &r)) {
		return Pool::MachineInteger(r);
	} else {
		return Pool::BigInteger(mpz_class(long(x.value)) + long(y.value));
	}
}

inline BaseExpressionRef operator*(const MachineInteger &x, const MachineInteger &y) {
	long r;
	if (!__builtin_smull_overflow(x.value, y.value, &r)) {
		return Pool::MachineInteger(r);
	} else {
		return Pool::BigInteger(mpz_class(long(x.value)) * long(y.value));
	}
}

inline BaseExpressionRef operator+(const MachineReal &x, const MachineReal &y) {
	return Pool::MachineReal(x.value + y.value);
}

inline BaseExpressionRef operator+(const BigInteger &x, const BigInteger &y) {
	return Pool::BigInteger(x.value + y.value);
}

inline BaseExpressionRef operator+(const BigInteger &x, const MachineInteger &y) {
	return Pool::BigInteger(x.value + long(y.value));
}

inline BaseExpressionRef operator+(const MachineInteger &x, const BigInteger &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const BigInteger &x, const MachineReal &y) {
	return Pool::MachineReal(x.value.get_d() + y.value);
}

inline BaseExpressionRef operator+(const MachineReal &x, const BigInteger &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const BigInteger &x, const BigReal &y) {
	arf_t temp;
	arf_init(temp);
	arf_set_mpz(temp, x.value.get_mpz_t());

	arb_t r;
	arb_init(r);
	arb_add_arf(r, y.value, temp, y.prec.bits);

	arf_clear(temp);

	return Pool::BigReal(r, y.prec);
}

inline BaseExpressionRef operator+(const BigReal &x, const BigInteger &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const BigRational &x, const BigInteger &y) {
	return Pool::BigRational(x.value + y.value);
}

inline BaseExpressionRef operator+(const BigInteger &x, const BigRational &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const BigRational &x, const MachineInteger &y) {
	return Pool::BigRational(x.value + long(y.value));
}

inline BaseExpressionRef operator+(const MachineInteger &x, const BigRational &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const MachineReal &x, const BigRational &y) {
	return Pool::MachineReal(x.value + y.value.get_d());
}

inline BaseExpressionRef operator+(const BigRational &x, const MachineReal &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const MachineReal &x, const MachineInteger &y) {
	return Pool::MachineReal(x.value + y.value);
}

inline BaseExpressionRef operator+(const MachineInteger &x, const MachineReal &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const MachineInteger &x, const BigReal &y) {
	arb_t r;
	arb_init(r);
	arb_add_si(r, y.value, long(x.value), y.prec.bits);
	return Pool::BigReal(r, y.prec);
}

inline BaseExpressionRef operator+(const BigReal &x, const MachineInteger &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const MachineReal &x, const BigReal &y) {
	return Pool::MachineReal(x.value + y.as_double());
}

inline BaseExpressionRef operator+(const BigReal &x, const MachineReal &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const BigReal &x, const BigReal &y) {
	arb_t r;
	arb_init(r);
	arb_add(r, x.value, y.value, std::min(x.prec.bits, y.prec.bits));
	return Pool::BigReal(r, x.prec.bits < y.prec.bits ? x.prec : y.prec);
}

inline BaseExpressionRef operator+(const BigRational &x, const BigReal &y) {
	arf_t num;
	arf_init(num);
	arf_set_mpz(num, x.value.get_num().get_mpz_t());

	arf_t den;
	arf_init(den);
	arf_set_mpz(den, x.value.get_den().get_mpz_t());

	arf_t q;
	arf_init(q);
	arf_div(q, num, den, y.prec.bits, ARF_RND_DOWN);

	arb_t r;
	arb_init(r);
	arb_add_arf(r, y.value, q, y.prec.bits);

	arf_clear(num);
	arf_clear(den);
	arf_clear(q);

	return Pool::BigReal(r, y.prec);
}

inline BaseExpressionRef operator+(const BigReal &x, const BigRational &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const BigRational &x, const BigRational &y) {
	return Pool::BigRational(x.value + y.value);
}

inline BaseExpressionRef operator*(const BigInteger &x, const BigInteger &y) {
	return Pool::BigInteger(x.value * y.value);
}

inline BaseExpressionRef operator*(const BigInteger &x, const MachineInteger &y) {
	return Pool::BigInteger(x.value * long(y.value));
}

inline BaseExpressionRef operator*(const MachineInteger &x, const BigInteger &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const BigRational &x, const MachineInteger &y) {
	const mpq_class q(x.value * long(y.value));
	if (q.get_den() == 1) {
		return from_primitive(q.get_num());
	} else {
		return Pool::BigRational(q);
	}
}

inline BaseExpressionRef operator*(const MachineInteger &x, const BigRational &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const BigRational &x, const BigInteger &y) {
	const mpq_class q(x.value * y.value);
	if (q.get_den() == 1) {
		return from_primitive(q.get_num());
	} else {
		return Pool::BigRational(q);
	}
}

inline BaseExpressionRef operator*(const BigInteger &x, const BigRational &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const MachineReal &x, const BigRational &y) {
	return Pool::MachineReal(x.value * y.value.get_d());
}

inline BaseExpressionRef operator*(const BigRational &x, const MachineReal &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const MachineReal &x, const MachineInteger &y) {
	return Pool::MachineReal(x.value * y.value);
}

inline BaseExpressionRef operator*(const MachineInteger &x, const MachineReal &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const MachineReal &x, const MachineReal &y) {
	return Pool::MachineReal(x.value * y.value);
}

inline BaseExpressionRef operator*(const MachineInteger &x, const BigReal &y) {
	arb_t r;
	arb_init(r);
	arb_mul_si(r, y.value, long(x.value), y.prec.bits);
	return Pool::BigReal(r, y.prec);
}

inline BaseExpressionRef operator*(const BigReal &x, const MachineInteger &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const BigInteger &x, const MachineReal &y) {
	return Pool::MachineReal(x.value.get_d() * y.value);
}

inline BaseExpressionRef operator*(const MachineReal &x, const BigInteger &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const MachineReal &x, const BigReal &y) {
	return Pool::MachineReal(x.value * y.as_double());
}

inline BaseExpressionRef operator*(const BigReal &x, const MachineReal &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const BigReal &x, const BigReal &y) {
	arb_t r;
	arb_init(r);
	arb_mul(r, x.value, y.value, std::min(x.prec.bits, y.prec.bits));
	return Pool::BigReal(r, x.prec.bits < y.prec.bits ? x.prec : y.prec);
}

inline BaseExpressionRef operator*(const BigInteger &x, const BigReal &y) {
	arf_t temp;
	arf_init(temp);
	arf_set_mpz(temp, x.value.get_mpz_t());

	arb_t r;
	arb_init(r);
	arb_mul_arf(r, y.value, temp, y.prec.bits);

	arf_clear(temp);

	return Pool::BigReal(r, y.prec);
}

inline BaseExpressionRef operator*(const BigReal &x, const BigInteger &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const BigRational &x, const BigReal &y) {
	arf_t num;
	arf_init(num);
	arf_set_mpz(num, x.value.get_num().get_mpz_t());

	arf_t den;
	arf_init(den);
	arf_set_mpz(den, x.value.get_den().get_mpz_t());

	arf_t q;
	arf_init(q);
	arf_div(q, num, den, y.prec.bits, ARF_RND_DOWN);

	arb_t r;
	arb_init(r);
	arb_mul_arf(r, y.value, q, y.prec.bits);

	arf_clear(num);
	arf_clear(den);
	arf_clear(q);

	return Pool::BigReal(r, y.prec);
}

inline BaseExpressionRef operator*(const BigReal &x, const BigRational &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const BigRational &x, const BigRational &y) {
	return Pool::BigRational(x.value * y.value);
}

struct plus {
	template<typename U, typename V>
	static inline BaseExpressionRef function(const U &u, const V &v, const Evaluation &) {
		return u + v;
	}

	static inline BaseExpressionRef fallback(
		const ExpressionPtr expr,
		const Evaluation &evaluation) {

		return add(expr, evaluation);
	}
};

struct times {
	template<typename U, typename V>
	static inline BaseExpressionRef function(const U &u, const V &v, const Evaluation &) {
		return u * v;
	}

	static inline BaseExpressionRef fallback(
		const ExpressionPtr expr,
		const Evaluation &evaluation) {

		return mul(expr, evaluation);
	}
};

inline const BaseExpression *if_divisor(const BaseExpression *b_base) {
	const Expression *b = static_cast<const Expression*>(b_base);
	if (b->_head->extended_type() != SymbolPower || b->size() != 2) {
		return nullptr;
	}

	const BaseExpressionRef *args = b->static_leaves<2>();
	if (!is_minus_1(args[1])) {
		return nullptr;
	}

	return args[0].get();
}

class TimesArithmetic : public BinaryArithmetic<times> {
public:
    using Base = BinaryArithmetic<times>;

	TimesArithmetic(const Definitions& definitions) : Base(definitions) {
		// detect Times[x, Power[y, -1]] and use fast divide if possible.

		/*BinaryOperator<times>::template init<MachineInteger, Expression>(
			[] (const BaseExpression *a, const BaseExpression *b) {

				const BaseExpression *divisor = if_divisor(b);
				if (!divisor) {
					return BaseExpressionRef(); // leave this for SymEngine to evaluate
				}

				switch (divisor->type()) {
					case MachineIntegerType: {
						const machine_integer_t x =
							static_cast<const MachineInteger*>(a)->value;
						const machine_integer_t y =
							static_cast<const MachineInteger*>(divisor)->value;
						const auto r = std::div(x, y);
						if (r.rem == 0) {
							return Pool::MachineInteger(r.quot);
						} else {
							return Pool::BigRational(x, y);
						}
					}

					case MachineRealType: {
						const machine_integer_t x =
							static_cast<const MachineInteger*>(a)->value;
						const machine_real_t y =
							static_cast<const MachineReal*>(divisor)->value;
						return Pool::MachineReal(x / y);
					}

					default:
						break;
				}

				return BaseExpressionRef(); // leave this for SymEngine to evaluate
			}
		);

		BinaryOperator<times>::template init<MachineReal, Expression>(
			[] (const BaseExpression *a, const BaseExpression *b) {

				const BaseExpression *divisor = if_divisor(b);
				if (!divisor) {
					return BaseExpressionRef(); // leave this for SymEngine to evaluate
				}

				switch (divisor->type()) {
					case MachineIntegerType: {
						const machine_integer_t x =
							static_cast<const MachineReal*>(a)->value;
						const machine_integer_t y =
							static_cast<const MachineInteger*>(divisor)->value;
						return Pool::MachineReal(x / y);
					}

					case MachineRealType: {
						const machine_real_t x =
							static_cast<const MachineReal*>(a)->value;
						const machine_real_t y =
							static_cast<const MachineReal*>(divisor)->value;
						return Pool::MachineReal(x / y);
					}

					default:
						break;
				}

				return BaseExpressionRef(); // leave this for SymEngine to evaluate
			}
		);*/
	}
};

/*constexpr auto Plus0 = NewRule<EmptyConstantRule<0>>;
constexpr auto Plus1 = NewRule<IdentityRule>;
constexpr auto Plus2 = NewRule<BinaryOperatorRule<BinaryArithmetic<plus>>>;
constexpr auto PlusN = NewRule<PlusNRule>;*/

constexpr auto Times0 = NewRule<EmptyConstantRule<1>>;
constexpr auto Times1 = NewRule<IdentityRule>;
constexpr auto Times2 = NewRule<BinaryOperatorRule<TimesArithmetic>>;
constexpr auto TimesN = NewRule<TimesNRule>;

class Plus : public Builtin {
public:
	static constexpr const char *name = "Plus";

	static constexpr const char *docs = R"(
        >> 1 + 2
         = 3
	)";

private:
    CachedBaseExpressionRef m_plus;
    CachedBaseExpressionRef m_minus;
    CachedBaseExpressionRef m_precedence;

public:
	using Builtin::Builtin;

	static constexpr auto attributes =
		Attributes::Flat + Attributes::Listable + Attributes::NumericFunction +
        Attributes::OneIdentity + Attributes::Orderless + Attributes::Protected;


	inline BaseExpressionRef do_format(
		const BaseExpressionRef *leaves,
        size_t n,
        const Evaluation &evaluation) {

        const ExpressionRef ops = expression(evaluation.List, sequential(
            [this, leaves, n, &evaluation] (auto &store) {
                for (size_t i = 1; i < n; i++) {
                    store(leaves[i]->is_negative() ? m_minus : m_plus);
                }
            }, std::max(index_t(0), index_t(n) - 1)));

        const BaseExpressionRef values = ops->with_slice(
            [this, leaves, n, &evaluation] (const auto &ops) {
                return expression(evaluation.List, sequential(
                    [this, leaves, n, &evaluation, &ops] (auto &store) {
	                    if (n > 0) {
		                    store(expression(evaluation.HoldForm, leaves[0]));
	                    }

                        for (size_t i = 1; i < n; i++) {
                            store(expression(
                                evaluation.HoldForm,
                                ops[i - 1] == m_plus ?
                                    leaves[i] :
                                    leaves[i]->negate(evaluation)));
                        }
                    }, n));
                });

        return expression(evaluation.Infix, values, ops, m_precedence, evaluation.Left);
    }

	void build(Runtime &runtime) {
        m_plus.initialize(Pool::String(std::string("+")));
        m_minus.initialize(Pool::String(std::string("-")));
		m_precedence.initialize(Pool::MachineInteger(310));

        builtin<EmptyConstantRule<0>>();
        builtin<IdentityRule>();
        builtin<BinaryOperatorRule<BinaryArithmetic<plus>>>();
        builtin<PlusNRule>();

        format(&Plus::do_format, runtime.symbols().All);
	}
};

class Divide : public Builtin {
public:
	static constexpr const char *name = "Divide";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Divide[$a$, $b$]'</dt>
    <dt>'$a$ / $b$'</dt>
        <dd>represents the division of $a$ by $b$.
    </dl>
    >> 30 / 5
     = 6
    >> 1 / 8
     = 1 / 8
    >> Pi / 4
     = Pi / 4

    Use 'N' or a decimal point to force numeric evaluation:
    #> Pi / 4.0
     = 0.785398
    >> 1 / 8
     = 1 / 8
    #> N[%]
     = 0.125

    Nested divisions:
    >> a / b / c
     = a / (b c)
    >> a / (b / c)
     = a c / b
    >> a / b / (c / (d / e))
     = a d / (b c e)
    >> a / (b ^ 2 * c ^ 3 / e)
     = a e / (b ^ 2 c ^ 3)

    #> 1 / 4.0
     = 0.25
    #> 10 / 3 // FullForm
     = Rational[10, 3]
    #> a / b // FullForm
     = Times[a, Power[b, -1]]
	)";

public:
	using Builtin::Builtin;

	static constexpr auto attributes =
		Attributes::Listable + Attributes::NumericFunction;

	void build(Runtime &runtime) {
		builtin("Divide[x_, y_]", "Times[x, Power[y, -1]]");
	}
};

class PowerRule : public ExactlyNRule<2> {
public:
	using ExactlyNRule<2>::ExactlyNRule;

	virtual BaseExpressionRef try_apply(
        const Expression *expr, const Evaluation &evaluation) const {

		/*const BaseExpressionRef *refs = expr->static_leaves<2>();
		const BaseExpressionRef &a = refs[0];
		const BaseExpressionRef &b = refs[1];

		if (b->type() == MachineIntegerType) {
			const machine_integer_t integer_exponent =
				static_cast<const MachineInteger*>(b.get())->value;
		}*/

		return expr->symbolic_evaluate_binary(SymEngine::pow, evaluation);
	}
};

constexpr auto Power = NewRule<PowerRule>;

class Subtract : public Builtin {
public:
	static constexpr const char *name = "Subtract";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Subtract[$a$, $b$]'</dt>
    <dt>$a$ - $b$</dt>
        <dd>represents the subtraction of $b$ from $a$.</dd>
    </dl>

    >> 5 - 3
     = 2
    >> a - b // FullForm
     = Plus[a, Times[-1, b]]
    >> a - b - c
     = a - b - c
    #> a - (b - c)
     = a - b + c
	)";

public:
	using Builtin::Builtin;

	static constexpr auto attributes =
		Attributes::Listable + Attributes::NumericFunction;

	void build(Runtime &runtime) {
		builtin("Subtract[x_, y_]", "Plus[x, Times[-1, y]]");
	}
};

class Minus : public Builtin {
public:
	static constexpr const char *name = "Minus";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Minus[$expr$]'
        <dd> is the negation of $expr$.
    </dl>

    >> -a //FullForm
     = Times[-1, a]

    'Minus' automatically distributes:
    >> -(x - 2/3)
     = 2 / 3 - x

    'Minus' threads over lists:
    >> -Range[10]
    = {-1, -2, -3, -4, -5, -6, -7, -8, -9, -10}
	)";

public:
	using Builtin::Builtin;

	static constexpr auto attributes =
		Attributes::Listable + Attributes::NumericFunction;

	void build(Runtime &runtime) {
		builtin("Minus[x_]", "Times[-1, x]");
	}
};

class Sqrt : public Builtin {
public:
	static constexpr const char *name = "Sqrt";

	static constexpr auto attributes =
		Attributes::Listable + Attributes::NumericFunction;

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Sqrt[$expr$]'
        <dd>returns the square root of $expr$.
    </dl>

    >> Sqrt[4]
     = 2
    >> Sqrt[5]
     = Sqrt[5]
    >> Sqrt[5] // N
     = 2.23607
    >> Sqrt[a]^2
     = a

    Complex numbers:
    >> Sqrt[-4]
     = 2 I
    >> I == Sqrt[-1]
     = True

    >> Plot[Sqrt[a^2], {a, -2, 2}]
     = -Graphics-

    #> N[Sqrt[2], 50]
     = 1.4142135623730950488016887242096980785696718753769
    )";

	using Builtin::Builtin;

	void build(Runtime &runtime) {
        builtin("Sqrt[x_]", "x ^ (1 / 2)");
	}
};

class Infinity : public Builtin {
public:
	static constexpr const char *name = "Infinity";

	static constexpr auto attributes =
		Attributes::Constant + Attributes::ReadProtected;

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Infinity'
        <dd>represents an infinite real quantity.
    </dl>

    >> 1 / Infinity
     = 0
    >> Infinity + 100
     = Infinity

    Use 'Infinity' in sum and limit calculations:
    #> Sum[1/x^2, {x, 1, Infinity}]
     = Pi ^ 2 / 6

    #> FullForm[Infinity]
     = DirectedInfinity[1]
    #> (2 + 3.5*I) / Infinity
     = 0. + 0. I
    >> Infinity + Infinity
     = Infinity
    #> Infinity / Infinity
     : Indeterminate expression 0 Infinity encountered.
     = Indeterminate
    )";

	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin("Infinity", "DirectedInfinity[1]");
	}
};

class ComplexInfinity : public Builtin {
public:
	static constexpr const char *name = "ComplexInfinity";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'ComplexInfinity'
        <dd>represents an infinite complex quantity of undetermined direction.
    </dl>

    >> 1 / ComplexInfinity
     = 0
    #> ComplexInfinity + ComplexInfinity
     = ComplexInfinity
    >> ComplexInfinity * Infinity
     = ComplexInfinity
    >> FullForm[ComplexInfinity]
     = DirectedInfinity[]
    )";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin("ComplexInfinity", "DirectedInfinity[]");
	}

};

class DirectedInfinity : public Builtin {
public:
	static constexpr const char *name = "DirectedInfinity";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&DirectedInfinity::apply_0);
		builtin(&DirectedInfinity::apply_1);
	}

	inline BaseExpressionRef apply_0(
        const EmptyExpression &empty,
		const Evaluation &evaluation) {

		empty.expr->symbolic_initialize([] () {
            return SymEngineRef(SymEngine::ComplexInf.get());
		}, evaluation);

		return BaseExpressionRef();
	}

	inline BaseExpressionRef apply_1(
		ExpressionPtr expr,
		BaseExpressionPtr x,
		const Evaluation &evaluation) {

		expr->symbolic_initialize([x] () {
			if (x->type() == MachineIntegerType) {
				const machine_integer_t direction =
					static_cast<const MachineInteger*>(x)->value;

				if (direction > 0) {
					return SymEngineRef(SymEngine::Inf.get());
				} else if (direction < 0) {
					return SymEngineRef(SymEngine::NegInf.get());
				}
			}

			return SymEngineRef();
		}, evaluation);

		return BaseExpressionRef();
	}
};

class Re : public Builtin {
public:
	static constexpr const char *name = "Re";

	static constexpr auto attributes = Attributes::Listable;

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Re[$z$]'
        <dd>returns the real component of the complex number $z$.
    </dl>

    >> Re[3+4I]
     = 3

    >> Plot[{Cos[a], Re[E^(I a)]}, {a, 0, 2 Pi}]
     = -Graphics-

    >> Im[0.5 + 2.3 I]
     = 2.3
    #> % // Precision
     = MachinePrecision
    )";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&Re::apply);
	}

	inline BaseExpressionRef apply(
		BaseExpressionPtr expr,
		const Evaluation &evaluation) {

		switch (expr->type()) {
			case MachineComplexType:
				return Pool::MachineReal(static_cast<const MachineComplex*>(expr)->value.real());

			case BigComplexType:
				return from_symbolic_form(
					static_cast<const BigComplex*>(expr)->m_value.get()->real_part(), evaluation);

			default:
				return BaseExpressionRef();
		}
	}
};

class Im : public Builtin {
public:
	static constexpr const char *name = "Im";

	static constexpr auto attributes = Attributes::Listable;

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Im[$z$]'
        <dd>returns the imaginary component of the complex number $z$.
    </dl>

    >> Im[3+4I]
     = 4

    >> Plot[{Sin[a], Im[E^(I a)]}, {a, 0, 2 Pi}]
     = -Graphics-

    >> Re[0.5 + 2.3 I]
     = 0.5
    #> % // Precision
     = MachinePrecision
    )";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&Im::apply);
	}

	inline BaseExpressionRef apply(
		BaseExpressionPtr expr,
		const Evaluation &evaluation) {

		switch (expr->type()) {
			case MachineComplexType:
				return Pool::MachineReal(static_cast<const MachineComplex*>(expr)->value.imag());

			case BigComplexType:
				return from_symbolic_form(
					static_cast<const BigComplex*>(expr)->m_value.get()->imaginary_part(), evaluation);

			default:
				return BaseExpressionRef();
		}
	}
};

class Conjugate : public Builtin {
public:
	static constexpr const char *name = "Conjugate";

	static constexpr auto attributes = Attributes::Listable;

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Conjugate[$z$]'
        <dd>returns the complex conjugate of the complex number $z$.
    </dl>

    >> Conjugate[3 + 4 I]
     = 3 - 4 I

    >> Conjugate[3]
     = 3

    #> Conjugate[a + b * I]
     = Conjugate[a] - I Conjugate[b]

    #> Conjugate[{{1, 2 + I 4, a + I b}, {I}}]
     = {{1, 2 - 4 I, Conjugate[a] - I Conjugate[b]}, {-I}}

    ## Issue #272
    #> {Conjugate[Pi], Conjugate[E]}
     = {Pi, E}

    >> Conjugate[1.5 + 2.5 I]
     = 1.5 - 2.5 I
    )";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
        builtin(&Conjugate::apply);
	}

	inline BaseExpressionRef apply(
		ExpressionPtr expr,
		BaseExpressionPtr x,
		const Evaluation &evaluation) {

        switch (x->type()) {
            case MachineIntegerType:
            case BigIntegerType:
            case MachineRationalType:
            case BigRationalType:
            case MachineRealType:
            case BigRealType:
                return x;
            case MachineComplexType:
                return static_cast<const MachineComplex*>(x)->conjugate();
            case BigComplexType:
                return static_cast<const BigComplex*>(x)->conjugate();
            default:
                return BaseExpressionRef();
        }
    }
};

class Abs : public Builtin {
public:
	static constexpr const char *name = "Abs";

	static constexpr auto attributes = Attributes::Listable;

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Abs[$x$]'
        <dd>returns the absolute value of $x$.
    </dl>
    >> Abs[-3]
     = 3

    'Abs' returns the magnitude of complex numbers:
    >> Abs[3 + I]
     = Sqrt[10]
    >> Abs[3.0 + I]
     = 3.16228
    >> Plot[Abs[x], {x, -4, 4}]
     = -Graphics-

    >> Abs[I]
     = 1
    >> Abs[a - b]
     = Abs[a - b]

    #> Abs[Sqrt[3]]
     = Sqrt[3]
    )";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&Abs::apply);
	}

	inline BaseExpressionRef apply(
		ExpressionPtr expr,
		BaseExpressionPtr x,
		const Evaluation &evaluation) {

		switch (x->type()) {
			case MachineIntegerType:
				return Pool::MachineInteger(std::abs(static_cast<const MachineInteger*>(x)->value));
			case MachineRealType:
				return Pool::MachineReal(std::abs(static_cast<const MachineReal*>(x)->value));
			default:
				return expr->symbolic_evaluate_unary(SymEngine::abs, evaluation);
		}
	}
};

class I : public Builtin {
public:
	static constexpr const char *name = "I";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'I'
        <dd>represents the imaginary number 'Sqrt[-1]'.
    </dl>

    >> I^2
     = -1
    >> (3+I)*(3-I)
     = 10
    )";

public:
	using Builtin::Builtin;

	void build(Runtime &runtime) {
		SymEngineComplexRef value = SymEngineComplexRef(
			new SymEngine::Complex(SymEngine::rational_class(0, 1), SymEngine::rational_class(1, 1)));
		runtime.definitions().lookup("System`I")->state().set_own_value(Pool::BigComplex(value));
	}
};

class NumberQ : public Builtin {
public:
	static constexpr const char *name = "NumberQ";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'NumberQ[$expr$]'
        <dd>returns 'True' if $expr$ is an explicit number, and 'False' otherwise.
    </dl>

    >> NumberQ[3+I]
     = True
    >> NumberQ[5!]
     = True
    >> NumberQ[Pi]
     = False
    )";

public:
    using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&NumberQ::test);
	}

    inline bool test(
        BaseExpressionPtr expr,
        const Evaluation &evaluation) {

	    return expr->is_number();
    }
};

class RealNumberQ : public Builtin {
public:
	static constexpr const char *name = "RealNumberQ";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'RealNumberQ[$expr$]'
        <dd>returns 'True' if $expr$ is an explicit number with no imaginary component.
    </dl>

    >> RealNumberQ[10]
     = True
    >> RealNumberQ[4.0]
     = True
    >> RealNumberQ[1+I]
     = False
    >> RealNumberQ[0 * I]
     = True
    >> RealNumberQ[0.0 * I]
     = False
    )";

public:
    using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&RealNumberQ::test);
	}

    inline bool test(
        BaseExpressionPtr expr,
        const Evaluation &evaluation) {

        switch (expr->type()) {
            case MachineIntegerType:
            case BigIntegerType:
            case MachineRealType:
            case BigRealType:
            case MachineRationalType:
            case BigRationalType:
                return true;
            default:
                return false;
        }
    }
};

class MachineNumberQ : public Builtin {
public:
	static constexpr const char *name = "MachineNumberQ";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'MachineNumberQ[$expr$]'
        <dd>returns 'True' if $expr$ is a machine-precision real or complex number.
    </dl>

    >> MachineNumberQ[3.14159265358979324]
     = False
    >> MachineNumberQ[1.5 + 2.3 I]
     = True
    >> MachineNumberQ[2.71828182845904524 + 3.14159265358979324 I]
     = False
    #> MachineNumberQ[1.5 + 3.14159265358979324 I]
     = True
    #> MachineNumberQ[1.5 + 5 I]
     = True
    )";

public:
    using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&MachineNumberQ::test);
	}

    inline bool test(
        BaseExpressionPtr expr,
        const Evaluation &evaluation) {

        switch (expr->type()) {
            case MachineRealType:
            case MachineComplexType:
                return true;
            default:
                return false;
        }
    }
};

class ExactNumberQ : public Builtin {
public:
	static constexpr const char *name = "ExactNumberQ";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'ExactNumberQ[$expr$]'
        <dd>returns 'True' if $expr$ is an exact number, and 'False' otherwise.
    </dl>

    >> ExactNumberQ[10]
     = True
    >> ExactNumberQ[4.0]
     = False
    >> ExactNumberQ[n]
     = False

    'ExactNumberQ' can be applied to complex numbers:
    >> ExactNumberQ[1 + I]
     = True
    >> ExactNumberQ[1 + 1. I]
     = False
    )";

public:
    using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&ExactNumberQ::test);
	}

    inline bool test(
        BaseExpressionPtr expr,
        const Evaluation &evaluation) {

        return expr->is_number() && !expr->is_inexact();
    }
};

class InexactNumberQ : public Builtin {
public:
	static constexpr const char *name = "InexactNumberQ";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'InexactNumberQ[$expr$]'
        <dd>returns 'True' if $expr$ is not an exact number, and 'False' otherwise.
    </dl>

    >> InexactNumberQ[a]
     = False
    >> InexactNumberQ[3.0]
     = True
    >> InexactNumberQ[2/3]
     = False

    'InexactNumberQ' can be applied to complex numbers:
    >> InexactNumberQ[4.0+I]
     = True
    )";

public:
    using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&InexactNumberQ::test);
	}

    inline bool test(
        BaseExpressionPtr expr,
        const Evaluation &evaluation) {

        return expr->is_number() && expr->is_inexact();
    }
};

class IntegerQ : public Builtin {
public:
	static constexpr const char *name = "IntegerQ";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'IntegerQ[$expr$]'
        <dd>returns 'True' if $expr$ is an integer, and 'False' otherwise.
    </dl>

    >> IntegerQ[3]
     = True
    >> IntegerQ[Pi]
     = False
    )";

public:
    using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&IntegerQ::test);
	}

    inline bool test(
        BaseExpressionPtr expr,
        const Evaluation &evaluation) {

        switch (expr->type()) {
            case MachineIntegerType:
            case BigIntegerType:
                return true;
            default:
                return false;
        }
    }
};

class Factorial : public Builtin {
public:
	static constexpr const char *name = "Factorial";

public:
    using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&Factorial::apply);
	}

    inline BaseExpressionRef apply(
        BaseExpressionPtr expr,
        const Evaluation &evaluation) {

        switch (expr->type()) {
            case MachineIntegerType: {
                mpz_t x;
                mpz_init(x);

                try {
                    const auto n = static_cast<const MachineInteger*>(expr)->value;
                    mpz_fac_ui(x, n);
                    const BaseExpressionRef result = from_primitive(mpz_class(x));
                    mpz_clear(x);
                    return result;
                } catch(...) {
                    mpz_clear(x);
                    throw;
                }
            }

            default:
                break; // nothing
        }

        return BaseExpressionRef();
    }
};

class Gamma : public Builtin {
public:
	static constexpr const char *name = "Gamma";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Gamma[$z$]'
        <dd>is the gamma function on the complex number $z$.
    <dt>'Gamma[$z$, $x$]'
        <dd>is the upper incomplete gamma function.
    <dt>'Gamma[$z$, $x0$, $x1$]'
        <dd>is equivalent to 'Gamma[$z$, $x0$] - Gamma[$z$, $x1$]'.
    </dl>

    'Gamma[$z$]' is equivalent to '($z$ - 1)!':
    #> Simplify[Gamma[z] - (z - 1)!]
     = 0

    Exact arguments:
    >> Gamma[8]
     = 5040
    >> Gamma[1/2]
     = Sqrt[Pi]
    >> Gamma[1, x]
     = E ^ (-x)
    #> Gamma[0, x]
     = ExpIntegralE[1, x]

    Numeric arguments:
    >> Gamma[123.78]
     = 4.21078*^204
    #> Gamma[1. + I]
     = 0.498016 - 0.15495 I

    Both 'Gamma' and 'Factorial' functions are continuous:
    >> Plot[{Gamma[x], x!}, {x, 0, 4}]
     = -Graphics-

    ## Issue 203
    #> N[Gamma[24/10], 100]
     = 1.242169344504305404913070252268300492431517240992022966055507541481863694148882652446155342679460339
    #> N[N[Gamma[24/10],100]/N[Gamma[14/10],100],100]
     = 1.400000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
    #> % // Precision
     = 100.

    #> Gamma[1.*^20]
     : Overflow occurred in computation.
     = Overflow[]

    ## Needs mpmath support for lowergamma
    #> Gamma[1., 2.]
     = Gamma[1., 2.]
    )";

public:
    using Builtin::Builtin;

	void build(Runtime &runtime) {
		builtin(&Gamma::apply_1);
		builtin(&Gamma::apply_2);
	}

	inline BaseExpressionRef apply_1(
		ExpressionPtr expr,
		BaseExpressionPtr x,
		const Evaluation &evaluation) {

		return expr->symbolic_evaluate_unary(SymEngine::gamma, evaluation);
	}

	inline BaseExpressionRef apply_2(
		ExpressionPtr expr,
		BaseExpressionPtr x,
		BaseExpressionPtr y,
		const Evaluation &evaluation) {

		return expr->symbolic_evaluate_binary(SymEngine::uppergamma, evaluation);
	}
};

class Pochhammer : public Builtin {
public:
	static constexpr const char *name = "Pochhammer";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Pochhammer[$a$, $n$]'
        <dd>is the Pochhammer symbol (a)_n.
    </dl>

    >> Pochhammer[4, 8]
     = 6652800
    )";

public:
    using Builtin::Builtin;

	void build(Runtime &runtime) {
        builtin("Pochhammer[a_, n_]", "Gamma[a + n] / Gamma[a]");
	}
};

class HarmonicNumber : public Builtin {
public:
	static constexpr const char *name = "HarmonicNumber";

	static constexpr const char *docs = R"(
    <dl>
    <dt>'HarmonicNumber[n]'
      <dd>returns the $n$th harmonic number.
    </dl>

    >> Table[HarmonicNumber[n], {n, 8}]
     = {1, 3 / 2, 11 / 6, 25 / 12, 137 / 60, 49 / 20, 363 / 140, 761 / 280}

    #> HarmonicNumber[3.8]
     = 2.03806

    #> HarmonicNumber[-1.5]
     = 0.613706
    )";

public:
    using Builtin::Builtin;

	void build(Runtime &runtime) {
        builtin("HarmonicNumber[-1]", "ComplexInfinity");
        builtin(&HarmonicNumber::apply);
	}

    inline BaseExpressionRef apply(
        BaseExpressionPtr expr,
        const Evaluation &evaluation) {

        if (expr->type() == MachineIntegerType) {
            return from_symbolic_form(SymEngine::harmonic(
                static_cast<const MachineInteger*>(expr)->value), evaluation);
        } else {
            return BaseExpressionRef();
        }
    }
};

class Boole : public Builtin {
public:
	static constexpr const char *name = "Boole";

	static constexpr auto attributes = Attributes::Listable;

	static constexpr const char *docs = R"(
    <dl>
    <dt>'Boole[expr]'
      <dd>returns 1 if expr is True and 0 if expr is False.
    </dl>

    >> Boole[2 == 2]
     = 1
    >> Boole[7 < 5]
     = 0
    >> Boole[a == 7]
     = Boole[a == 7]
    )";

public:
    using Builtin::Builtin;

	void build(Runtime &runtime) {
        builtin(&Boole::apply);
	}

    inline BaseExpressionRef apply(
        BaseExpressionPtr expr,
        const Evaluation &evaluation) {

        switch (expr->extended_type()) {
            case SymbolTrue:
                return evaluation.one;
            case SymbolFalse:
                return evaluation.zero;
            default:
                return BaseExpressionRef();
        }
    }
};

void Builtins::Arithmetic::initialize() {

    add<Plus>();

	add("Times",
		Attributes::Flat + Attributes::Listable + Attributes::NumericFunction +
			Attributes::OneIdentity + Attributes::Orderless + Attributes::Protected, {
		    Times0,
		    Times1,
			Times2,
			TimesN
	    });

    add<Divide>();

	add("Power",
		Attributes::Listable + Attributes::NumericFunction + Attributes::OneIdentity, {
			Power
	    });

    add<Subtract>();
	add<Minus>();

    add<Sqrt>();
	add<Infinity>();
    add<ComplexInfinity>();
	add<DirectedInfinity>();

	add<Re>();
	add<Im>();
	add<Conjugate>();
	add<Abs>();
	add<I>();

	add<NumberQ>();
    add<RealNumberQ>();
    add<MachineNumberQ>();
    add<ExactNumberQ>();
    add<InexactNumberQ>();
    add<IntegerQ>();

	add<Factorial>();
	add<Gamma>();
	add<Pochhammer>();
    add<HarmonicNumber>();

    add<Boole>();
}
