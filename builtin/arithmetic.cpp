#include "arithmetic.h"
#include "../core/definitions.h"

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
struct Comparison<Rational, MachineInteger> {
	template<typename F>
	inline static bool compare(const Rational &u, const MachineInteger &v, const F &f) {
		return f(u.value, long(v.value));
	}
};

template<>
struct Comparison<MachineInteger, Rational> {
	template<typename F>
	inline static bool compare(const MachineInteger &u, const Rational &v, const F &f) {
		return f(long(u.value), v.value);
	}
};

inline BaseExpressionRef operator+(const MachineInteger &x, const MachineInteger &y) {
	long r;
	if (!__builtin_saddl_overflow(x.value, y.value, &r)) {
		return Heap::MachineInteger(r);
	} else {
		return Heap::BigInteger(mpz_class(long(x.value)) + long(y.value));
	}
}

inline BaseExpressionRef operator*(const MachineInteger &x, const MachineInteger &y) {
	long r;
	if (!__builtin_smull_overflow(x.value, y.value, &r)) {
		return Heap::MachineInteger(r);
	} else {
		return Heap::BigInteger(mpz_class(long(x.value)) * long(y.value));
	}
}

inline BaseExpressionRef operator+(const MachineReal &x, const MachineReal &y) {
	return Heap::MachineReal(x.value + y.value);
}

inline BaseExpressionRef operator+(const BigInteger &x, const BigInteger &y) {
	return Heap::BigInteger(x.value + y.value);
}

inline BaseExpressionRef operator+(const BigInteger &x, const MachineInteger &y) {
	return Heap::BigInteger(x.value + long(y.value));
}

inline BaseExpressionRef operator+(const MachineInteger &x, const BigInteger &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const BigInteger &x, const MachineReal &y) {
	return Heap::MachineReal(x.value.get_d() + y.value);
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

	return Heap::BigReal(r, y.prec);
}

inline BaseExpressionRef operator+(const BigReal &x, const BigInteger &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const Rational &x, const BigInteger &y) {
	return Heap::Rational(x.value + y.value);
}

inline BaseExpressionRef operator+(const BigInteger &x, const Rational &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const Rational &x, const MachineInteger &y) {
	return Heap::Rational(x.value + long(y.value));
}

inline BaseExpressionRef operator+(const MachineInteger &x, const Rational &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const MachineReal &x, const Rational &y) {
	return Heap::MachineReal(x.value + y.value.get_d());
}

inline BaseExpressionRef operator+(const Rational &x, const MachineReal &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const MachineReal &x, const MachineInteger &y) {
	return Heap::MachineReal(x.value + y.value);
}

inline BaseExpressionRef operator+(const MachineInteger &x, const MachineReal &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const MachineInteger &x, const BigReal &y) {
	arb_t r;
	arb_init(r);
	arb_add_si(r, y.value, long(x.value), y.prec.bits);
	return Heap::BigReal(r, y.prec);
}

inline BaseExpressionRef operator+(const BigReal &x, const MachineInteger &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const MachineReal &x, const BigReal &y) {
	return Heap::MachineReal(x.value + y.as_double());
}

inline BaseExpressionRef operator+(const BigReal &x, const MachineReal &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const BigReal &x, const BigReal &y) {
	arb_t r;
	arb_init(r);
	arb_add(r, x.value, y.value, std::min(x.prec.bits, y.prec.bits));
	return Heap::BigReal(r, x.prec.bits < y.prec.bits ? x.prec : y.prec);
}

inline BaseExpressionRef operator+(const Rational &x, const BigReal &y) {
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

	return Heap::BigReal(r, y.prec);
}

inline BaseExpressionRef operator+(const BigReal &x, const Rational &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const Rational &x, const Rational &y) {
	return Heap::Rational(x.value + y.value);
}

inline BaseExpressionRef operator*(const BigInteger &x, const BigInteger &y) {
	return Heap::BigInteger(x.value * y.value);
}

inline BaseExpressionRef operator*(const BigInteger &x, const MachineInteger &y) {
	return Heap::BigInteger(x.value * long(y.value));
}

inline BaseExpressionRef operator*(const MachineInteger &x, const BigInteger &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const Rational &x, const MachineInteger &y) {
	const mpq_class q(x.value * long(y.value));
	if (q.get_den() == 1) {
		return from_primitive(q.get_num());
	} else {
		return Heap::Rational(q);
	}
}

inline BaseExpressionRef operator*(const MachineInteger &x, const Rational &y) {
	return y + x;
}

inline BaseExpressionRef operator*(const Rational &x, const BigInteger &y) {
	const mpq_class q(x.value * y.value);
	if (q.get_den() == 1) {
		return from_primitive(q.get_num());
	} else {
		return Heap::Rational(q);
	}
}

inline BaseExpressionRef operator*(const BigInteger &x, const Rational &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const MachineReal &x, const Rational &y) {
	return Heap::MachineReal(x.value * y.value.get_d());
}

inline BaseExpressionRef operator*(const Rational &x, const MachineReal &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const MachineReal &x, const MachineInteger &y) {
	return Heap::MachineReal(x.value * y.value);
}

inline BaseExpressionRef operator*(const MachineInteger &x, const MachineReal &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const MachineReal &x, const MachineReal &y) {
	return Heap::MachineReal(x.value * y.value);
}

inline BaseExpressionRef operator*(const MachineInteger &x, const BigReal &y) {
	arb_t r;
	arb_init(r);
	arb_mul_si(r, y.value, long(x.value), y.prec.bits);
	return Heap::BigReal(r, y.prec);
}

inline BaseExpressionRef operator*(const BigReal &x, const MachineInteger &y) {
	return y + x;
}

inline BaseExpressionRef operator*(const BigInteger &x, const MachineReal &y) {
	return Heap::MachineReal(x.value.get_d() * y.value);
}

inline BaseExpressionRef operator*(const MachineReal &x, const BigInteger &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const MachineReal &x, const BigReal &y) {
	return Heap::MachineReal(x.value * y.as_double());
}

inline BaseExpressionRef operator*(const BigReal &x, const MachineReal &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const BigReal &x, const BigReal &y) {
	arb_t r;
	arb_init(r);
	arb_mul(r, x.value, y.value, std::min(x.prec.bits, y.prec.bits));
	return Heap::BigReal(r, x.prec.bits < y.prec.bits ? x.prec : y.prec);
}

inline BaseExpressionRef operator*(const BigInteger &x, const BigReal &y) {
	arf_t temp;
	arf_init(temp);
	arf_set_mpz(temp, x.value.get_mpz_t());

	arb_t r;
	arb_init(r);
	arb_mul_arf(r, y.value, temp, y.prec.bits);

	arf_clear(temp);

	return Heap::BigReal(r, y.prec);
}

inline BaseExpressionRef operator*(const BigReal &x, const BigInteger &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const Rational &x, const BigReal &y) {
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

	return Heap::BigReal(r, y.prec);
}

inline BaseExpressionRef operator*(const BigReal &x, const Rational &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const Rational &x, const Rational &y) {
	return Heap::Rational(x.value * y.value);
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
struct Comparison<Rational, BigReal> {
	template<typename F>
	inline static bool compare(const Rational &u, const BigReal &v, const F &f) {
		return f(Numeric::R(u.value, v.prec), Numeric::R(v.value));
	}
};

template<>
struct Comparison<BigReal, Rational> {
	template<typename F>
	inline static bool compare(const BigReal &u, const Rational &v, const F &f) {
		return f(Numeric::R(u.value), Numeric::R(v.value, u.prec));
	}
};

template<typename F>
class BinaryOperator {
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
	BinaryOperator() {
		std::memset(_functions, 0, sizeof(_functions));
	}

	inline BaseExpressionRef operator()(const Definitions &definitions, const BaseExpressionRef *leaves) const {
		const BaseExpression * const a = leaves[0].get();
		const BaseExpression * const b = leaves[1].get();
		const Function f = _functions[a->type() | (size_t(b->type()) << CoreTypeBits)];
		if (f) {
			return result(definitions, f(a, b));
		} else {
			return BaseExpressionRef();
		}
	}
};

template<typename F>
class BinaryArithmetic : public BinaryOperator<F> {
public:
	BinaryArithmetic() {
		BinaryOperator<F>::template init<MachineInteger, MachineInteger>();
		BinaryOperator<F>::template init<MachineInteger, BigInteger>();
		BinaryOperator<F>::template init<MachineInteger, Rational>();
		BinaryOperator<F>::template init<MachineInteger, MachineReal>();
		BinaryOperator<F>::template init<MachineInteger, BigReal>();

		BinaryOperator<F>::template init<BigInteger, MachineInteger>();
		BinaryOperator<F>::template init<BigInteger, BigInteger>();
		BinaryOperator<F>::template init<BigInteger, Rational>();
		BinaryOperator<F>::template init<BigInteger, MachineReal>();
		BinaryOperator<F>::template init<BigInteger, BigReal>();

		BinaryOperator<F>::template init<Rational, MachineInteger>();
		BinaryOperator<F>::template init<Rational, BigInteger>();
		BinaryOperator<F>::template init<Rational, MachineReal>();
		BinaryOperator<F>::template init<Rational, BigReal>();
		BinaryOperator<F>::template init<Rational, Rational>();

		BinaryOperator<F>::template init<MachineReal, MachineInteger>();
		BinaryOperator<F>::template init<MachineReal, BigInteger>();
		BinaryOperator<F>::template init<MachineReal, Rational>();
		BinaryOperator<F>::template init<MachineReal, MachineReal>();
		BinaryOperator<F>::template init<MachineReal, BigReal>();

		BinaryOperator<F>::template init<BigReal, MachineInteger>();
		BinaryOperator<F>::template init<BigReal, BigInteger>();
		BinaryOperator<F>::template init<BigReal, Rational>();
		BinaryOperator<F>::template init<BigReal, MachineReal>();
		BinaryOperator<F>::template init<BigReal, BigReal>();
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

inline bool is_minus_1(const BaseExpressionRef &expr) {
	if (expr->type() == MachineIntegerType) {
		return static_cast<const MachineInteger*>(expr.get())->value == -1;
	} else {
		return false;
	}
}

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
	TimesArithmetic() {
		// detect Times[x, Power[y, -1]] and use fast divide if possible.

		BinaryOperator<times>::template init<MachineInteger, Expression>(
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
								return Heap::MachineInteger(r.quot);
							} else {
								return Heap::Rational(x, y);
							}
						}

						case MachineRealType: {
							const machine_integer_t x =
									static_cast<const MachineInteger*>(a)->value;
							const machine_real_t y =
									static_cast<const MachineReal*>(divisor)->value;
							return Heap::MachineReal(x / y);
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
							return Heap::MachineReal(x / y);
						}

						case MachineRealType: {
							const machine_real_t x =
									static_cast<const MachineReal*>(a)->value;
							const machine_real_t y =
									static_cast<const MachineReal*>(divisor)->value;
							return Heap::MachineReal(x / y);
						}

						default:
							break;
					}

					return BaseExpressionRef(); // leave this for SymEngine to evaluate
				}
		);
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

		return Heap::MachineInteger(Value);
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
	const Operator _operator;

public:
	BinaryOperatorRule(const SymbolRef &head, const Definitions &definitions) :
			ExactlyNRule<2>(head, definitions), _operator() {
	}

	virtual BaseExpressionRef try_apply(
			const Expression *expr, const Evaluation &evaluation) const {

		return _operator(evaluation.definitions, expr->static_leaves<2>());
	}
};

constexpr auto Plus0 = NewRule<EmptyConstantRule<0>>;
constexpr auto Plus1 = NewRule<IdentityRule>;
constexpr auto Plus2 = NewRule<BinaryOperatorRule<BinaryArithmetic<plus>>>;

constexpr auto Times2 = NewRule<BinaryOperatorRule<TimesArithmetic>>;

class Plus3Rule : public AtLeastNRule<3> {
public:
	using AtLeastNRule<3>::AtLeastNRule;

	virtual BaseExpressionRef try_apply(
			const Expression *expr, const Evaluation &evaluation) const;
};

BaseExpressionRef Plus3Rule::try_apply(
	const Expression *expr, const Evaluation &evaluation) const {

	return expr->Plus();
}

constexpr auto Plus3 = NewRule<Plus3Rule>;

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

void Builtin::Arithmetic::initialize() {

	add("Plus",
	    Attributes::None, {
			Plus0,
		    Plus1,
		    Plus2,
		    Plus3
	    });

	add("Subtract",
	    Attributes::None, {
			rewrite("Subtract[x_, y_]", "Plus[x, Times[-1, y]]")
	    }
	);

	add("Times",
	    Attributes::None, {
			Times2
	    });

	add("Power",
	    Attributes::None, {
			Power
	    });

	add("Sqrt",
	    Attributes::None, {
			rewrite("Sqrt[x_]", "x ^ (1 / 2)")
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
}
