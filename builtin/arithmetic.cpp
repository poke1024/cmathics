// @formatter:off

#include "arithmetic.h"
#include "../core/definitions.h"
#include "../arithmetic/add.h"
#include "../arithmetic/mul.h"

static_assert(sizeof(machine_integer_t) == sizeof(long),
	"machine integer type must equivalent to long");

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

class no_binary_fallback {
public:
	inline BaseExpressionRef operator()(const Expression*, const Evaluation&) const {
		return BaseExpressionRef();
	}
};

template<typename F, typename Fallback = no_binary_fallback>
class BinaryOperator {
private:
	const Fallback m_fallback;

protected:
	typedef decltype(F::calculate(nullptr, nullptr)) IntermediateType;

	typedef std::function<IntermediateType(const BaseExpression *a, const BaseExpression *b)> Function;

	Function _functions[1LL << (2 * CoreTypeBits)];

	template<typename U, typename V>
	void init(const Function &f) {
		_functions[size_t(U::Type) | (size_t(V::Type) << CoreTypeBits)] = f;
	}

	template<typename U, typename V>
	void init() {
		init<U, V>([] (const BaseExpression *a, const BaseExpression *b) {
			return F::calculate(*static_cast<const U*>(a), *static_cast<const V*>(b));
		});
	};

	static inline BaseExpressionRef result(const Definitions &definitions, const BaseExpressionRef &result) {
		return result;
	}

	static inline BaseExpressionRef result(const Definitions &definitions, bool result) {
		return definitions.symbols().Boolean(result);
	}

public:
	BinaryOperator(const Fallback &fallback = Fallback()) : m_fallback(fallback) {
		std::memset(_functions, 0, sizeof(_functions));
	}

	inline BaseExpressionRef operator()(
		const Definitions &definitions,
		const Expression *expr,
		const Evaluation &evaluation) const {

		const BaseExpressionRef * const leaves = expr->static_leaves<2>();

		const BaseExpression * const a = leaves[0].get();
		const BaseExpression * const b = leaves[1].get();
		const Function f = _functions[a->type() | (size_t(b->type()) << CoreTypeBits)];

		if (f) {
			return result(definitions, f(a, b));
		} else {
			return m_fallback(expr, evaluation);
		}
	}
};

template<typename F, typename Fallback = no_binary_fallback>
class BinaryArithmetic : public BinaryOperator<F, Fallback> {
public:
	using Base = BinaryOperator<F, Fallback>;

	BinaryArithmetic(const Fallback &fallback = Fallback()) : Base(fallback) {

		Base::template init<MachineInteger, MachineInteger>();
		Base::template init<MachineInteger, BigInteger>();
		Base::template init<MachineInteger, BigRational>();
		Base::template init<MachineInteger, MachineReal>();
		Base::template init<MachineInteger, BigReal>();

		Base::template init<BigInteger, MachineInteger>();
		Base::template init<BigInteger, BigInteger>();
		Base::template init<BigInteger, BigRational>();
		Base::template init<BigInteger, MachineReal>();
		Base::template init<BigInteger, BigReal>();

		Base::template init<BigRational, MachineInteger>();
		Base::template init<BigRational, BigInteger>();
		Base::template init<BigRational, MachineReal>();
		Base::template init<BigRational, BigReal>();
		Base::template init<BigRational, BigRational>();

		Base::template init<MachineReal, MachineInteger>();
		Base::template init<MachineReal, BigInteger>();
		Base::template init<MachineReal, BigRational>();
		Base::template init<MachineReal, MachineReal>();
		Base::template init<MachineReal, BigReal>();

		Base::template init<BigReal, MachineInteger>();
		Base::template init<BigReal, BigInteger>();
		Base::template init<BigReal, BigRational>();
		Base::template init<BigReal, MachineReal>();
		Base::template init<BigReal, BigReal>();
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

struct plus {
	template<typename U, typename V>
	static BaseExpressionRef calculate(const U &u, const V &v) {
		return u + v;
	}
};

struct times {
	template<typename U, typename V>
	static BaseExpressionRef calculate(const U &u, const V &v) {
		return u * v;
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

class binary_times_fallback {
public:
	inline BaseExpressionRef operator()(const Expression *expr, const Evaluation &evaluation) const {
		return mul(expr, evaluation);
	}
};

class TimesArithmetic : public BinaryArithmetic<times, binary_times_fallback> {
public:
	TimesArithmetic() {
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

template<machine_integer_t Value>
class EmptyConstantRule : public ExactlyNRule<0> {
public:
	EmptyConstantRule(const SymbolRef &head, const Definitions &definitions) :
        ExactlyNRule<0>(head, definitions) {
	}

	virtual BaseExpressionRef try_apply(
        const Expression *expr, const Evaluation &evaluation) const {

		return Pool::MachineInteger(Value);
	}
};

class IdentityRule : public ExactlyNRule<1> {
public:
	IdentityRule(const SymbolRef &head, const Definitions &definitions) :
        ExactlyNRule<1>(head, definitions) {
	}

	virtual BaseExpressionRef try_apply(
        const Expression *expr, const Evaluation &evaluation) const {

		return expr->static_leaves<1>()[0];
	}
};

template<typename Operator>
class BinaryOperatorRule : public ExactlyNRule<2> {
private:
	const Operator m_operator;

public:
	BinaryOperatorRule(const SymbolRef &head, const Definitions &definitions) :
        ExactlyNRule<2>(head, definitions), m_operator() {
	}

	virtual BaseExpressionRef try_apply(
        const Expression *expr, const Evaluation &evaluation) const {

		return m_operator(evaluation.definitions, expr, evaluation);
	}
};

class binary_plus_fallback {
public:
	inline BaseExpressionRef operator()(const Expression *expr, const Evaluation &evaluation) const {
		return add(expr, evaluation);
	}
};

constexpr auto Plus0 = NewRule<EmptyConstantRule<0>>;
constexpr auto Plus1 = NewRule<IdentityRule>;
constexpr auto Plus2 = NewRule<BinaryOperatorRule<BinaryArithmetic<plus, binary_plus_fallback>>>;
constexpr auto PlusN = NewRule<PlusNRule>;

constexpr auto Times0 = NewRule<EmptyConstantRule<1>>;
constexpr auto Times1 = NewRule<IdentityRule>;
constexpr auto Times2 = NewRule<BinaryOperatorRule<TimesArithmetic>>;
constexpr auto TimesN = NewRule<TimesNRule>;

class PowerRule : public ExactlyNRule<2> {
public:
	using ExactlyNRule<2>::ExactlyNRule;

	virtual BaseExpressionRef try_apply(
        const Expression *expr, const Evaluation &evaluation) const {

		const BaseExpressionRef *refs = expr->static_leaves<2>();
		const BaseExpressionRef &a = refs[0];
		const BaseExpressionRef &b = refs[1];

		if (b->type() == MachineIntegerType) {
			const machine_integer_t integer_exponent =
					static_cast<const MachineInteger*>(b.get())->value;
		}

		return BaseExpressionRef();
	}
};

constexpr auto Power = NewRule<PowerRule>;

constexpr auto Less = NewRule<BinaryOperatorRule<BinaryArithmetic<less>>>;
constexpr auto LessEqual = NewRule<BinaryOperatorRule<BinaryArithmetic<less_equal>>>;
constexpr auto Greater = NewRule<BinaryOperatorRule<BinaryArithmetic<greater>>>;
constexpr auto GreaterEqual = NewRule<BinaryOperatorRule<BinaryArithmetic<greater_equal>>>;

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
    #> Infinity + Infinity
     = Infinity
    #> Infinity / Infinity
     : Indeterminate expression 0 Infinity encountered.
     = Indeterminate
    )";

	using Builtin::Builtin;

	void build(Runtime &runtime) {
		rule("Infinity", "DirectedInfinity[1]");
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
		rule("Subtract[x_, y_]", "Plus[x, Times[-1, y]]");
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
		rule("Minus[x_]", "Times[-1, x]");
	}
};

void Builtins::Arithmetic::initialize() {

	add("Plus",
	    Attributes::Flat + Attributes::Listable + Attributes::NumericFunction +
		    Attributes::OneIdentity + Attributes::Orderless + Attributes::Protected, {
			Plus0,
		    Plus1,
		    Plus2,
		    PlusN
	    }, R"(
			>> 1 + 2
			 = 3
		)");

	add<Subtract>();

	add<Minus>();

	add("Times",
		Attributes::Flat + Attributes::Listable + Attributes::NumericFunction +
			Attributes::OneIdentity + Attributes::Orderless + Attributes::Protected, {
		    Times0,
		    Times1,
			Times2,
			TimesN
	    });

	add("Power",
		Attributes::Listable + Attributes::NumericFunction + Attributes::OneIdentity, {
			Power
	    });

	add("Sqrt",
		Attributes::Listable + Attributes::NumericFunction, {
			down("Sqrt[x_]", "x ^ (1 / 2)")
	    }
	);

	add("Less",
	    Attributes::None, {
			Less
	    });

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

	add<Infinity>();

	add<NumberQ>();
    add<RealNumberQ>();
    add<MachineNumberQ>();

	add<Factorial>();
}
