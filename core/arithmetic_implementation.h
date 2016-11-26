#ifndef CMATHICS_ARITHMETIC_IMPL_H_H
#define CMATHICS_ARITHMETIC_IMPL_H_H

#include "arithmetic.h"
#include "integer.h"
#include "real.h"
#include "rational.h"
#include "expression.h"

template<typename F, typename U, typename V, typename W, bool ia, bool ib>
struct calculate {
};

template<typename F, typename U, typename V, typename W>
struct calculate<F, U, V, W, true, true> {
	inline auto operator()(const BaseExpression *a, const BaseExpression *b) const {
		return F::calculate(static_cast<const U *>(a)->value, static_cast<const V *>(b)->value);
	}
};

template<typename F, typename U, typename V, typename W>
struct calculate<F, U, V, W, false, true> {
	inline auto operator()(const BaseExpression *a, const BaseExpression *b) const {
		return F::calculate(promote<W>(static_cast<const U *>(a)->value), static_cast<const V *>(b)->value);
	}
};

template<typename F, typename U, typename V, typename W>
struct calculate<F, U, V, W, true, false> {
	inline auto operator()(const BaseExpression *a, const BaseExpression *b) const {
		return F::calculate(static_cast<const U *>(a)->value, promote<W>(static_cast<const V *>(b)->value));
	}
};

template<typename F, typename U, typename V, typename W>
struct calculate<F, U, V, W, false, false> {
	inline auto operator()(const BaseExpression *a, const BaseExpression *b) const {
		return F::calculate(promote<W>(static_cast<const U *>(a)->value), promote<W>(static_cast<const V *>(b)->value));
	}
};

template<typename F>
class BinaryOperator {
protected:
	typedef decltype(F::calculate(nullptr, nullptr)) IntermediateType;

	typedef std::function<IntermediateType(const BaseExpression *a, const BaseExpression *b)> Function;

	Function _functions[1LL << (2 * CoreTypeBits)];

	template<typename U, typename V, typename W>
	void init() {
		_functions[U::Type | (size_t(V::Type) << CoreTypeBits)] =
			[] (const BaseExpression *a, const BaseExpression *b) {
				constexpr bool ia = std::is_same<decltype(static_cast<const U*>(a)->value), const W>::value;
				constexpr bool ib = std::is_same<decltype(static_cast<const V*>(b)->value), const W>::value;
				return calculate<F, U, V, W, ia, ib>()(a, b);
			};
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
		BinaryOperator<F>::template init<MachineInteger, MachineInteger, mpint>();
		BinaryOperator<F>::template init<MachineInteger, BigInteger, mpint>();
		BinaryOperator<F>::template init<MachineInteger, MachineReal, mpfr::mpreal>();
		BinaryOperator<F>::template init<MachineInteger, BigReal, mpfr::mpreal>();

		BinaryOperator<F>::template init<BigInteger, MachineInteger, mpint>();
		BinaryOperator<F>::template init<BigInteger, BigInteger, mpz_class>();
		BinaryOperator<F>::template init<BigInteger, MachineReal, mpfr::mpreal>();
		BinaryOperator<F>::template init<BigInteger, BigReal, mpfr::mpreal>();

		BinaryOperator<F>::template init<MachineReal, MachineInteger, mpfr::mpreal>();
		BinaryOperator<F>::template init<MachineReal, BigInteger, mpfr::mpreal>();
		BinaryOperator<F>::template init<MachineReal, MachineReal, machine_real_t>();
		BinaryOperator<F>::template init<MachineReal, BigReal, mpfr::mpreal>();

		BinaryOperator<F>::template init<BigReal, MachineInteger, mpfr::mpreal>();
		BinaryOperator<F>::template init<BigReal, BigInteger, mpfr::mpreal>();
		BinaryOperator<F>::template init<BigReal, MachineReal, mpfr::mpreal>();
		BinaryOperator<F>::template init<BigReal, BigReal, mpfr::mpreal>();
	}};

template<typename F>
class BinaryComparison : public BinaryOperator<F> {
public:
	BinaryComparison() {
		BinaryOperator<F>::template init<MachineInteger, MachineInteger, machine_integer_t>();
		BinaryOperator<F>::template init<MachineInteger, BigInteger, mpz_class>();
		BinaryOperator<F>::template init<MachineInteger, MachineReal, mpfr::mpreal>();
		BinaryOperator<F>::template init<MachineInteger, BigReal, mpfr::mpreal>();

		BinaryOperator<F>::template init<BigInteger, MachineInteger, mpz_class>();
		BinaryOperator<F>::template init<BigInteger, BigInteger, mpz_class>();
		BinaryOperator<F>::template init<BigInteger, MachineReal, mpfr::mpreal>();
		BinaryOperator<F>::template init<BigInteger, BigReal, mpfr::mpreal>();

		BinaryOperator<F>::template init<MachineReal, MachineInteger, mpfr::mpreal>();
		BinaryOperator<F>::template init<MachineReal, BigInteger, mpfr::mpreal>();
		BinaryOperator<F>::template init<MachineReal, MachineReal, machine_real_t>();
		BinaryOperator<F>::template init<MachineReal, BigReal, mpfr::mpreal>();

		BinaryOperator<F>::template init<BigReal, MachineInteger, mpfr::mpreal>();
		BinaryOperator<F>::template init<BigReal, BigInteger, mpfr::mpreal>();
		BinaryOperator<F>::template init<BigReal, MachineReal, mpfr::mpreal>();
		BinaryOperator<F>::template init<BigReal, BigReal, mpfr::mpreal>();
	}
};

struct less {
	template<typename T>
	static bool calculate(const T &u, const T &v) {
		return u < v;
	}
};

struct greater {
	template<typename T>
	static bool calculate(const T &u, const T &v) {
		return u > v;
	}
};

struct plus {
	template<typename T>
	static BaseExpressionRef calculate(const T &u, const T &v) {
		const T x = u + v;
		return from_primitive(std::move(x));
	}
};

struct times {
	template<typename T>
	static BaseExpressionRef calculate(const T &u, const T &v) {
		const T x = u * v;
		return from_primitive(std::move(x));
	}
};

template<machine_integer_t Value>
class EmptyConstantRule : public ExactlyNRule<0> {
public:
	EmptyConstantRule(const SymbolRef &head, const Definitions &definitions) :
		ExactlyNRule<0>(head, definitions) {
	}

	virtual BaseExpressionRef try_apply(
		const ExpressionRef &expr, const Evaluation &evaluation) const {
		return Heap::MachineInteger(Value);
	}
};

class IdentityRule : public ExactlyNRule<1> {
public:
	IdentityRule(const SymbolRef &head, const Definitions &definitions) :
		ExactlyNRule<1>(head, definitions) {
	}

	virtual BaseExpressionRef try_apply(
		const ExpressionRef &expr, const Evaluation &evaluation) const {

		const StaticExpression<1>* expr1 = static_cast<const StaticExpression<1>*>(expr.get());
		return expr1->_leaves.refs()[0];
	}
};


template<typename Operator>
class BinaryOperatorRule : public ExactlyNRule<2> {
private:
	const Operator _operator;

public:
	BinaryOperatorRule(const SymbolRef &head, const Definitions &definitions) :
		ExactlyNRule<2>(head, definitions) {
	}

	virtual BaseExpressionRef try_apply(
		const ExpressionRef &expr, const Evaluation &evaluation) const {

		// match_size() guarantees that we receive a StaticExpression<2> here in try_apply().
		const StaticExpression<2>* expr2 = static_cast<const StaticExpression<2>*>(expr.get());
		return _operator(evaluation.definitions, expr2->_leaves.refs());
	}
};

template<typename T>
RuleRef NewRule(const SymbolRef &head, const Definitions &definitions) {
	return std::make_shared<T>(head, definitions);
}

constexpr auto Plus0 = NewRule<EmptyConstantRule<0>>;
constexpr auto Plus1 = NewRule<IdentityRule>;
constexpr auto Plus2 = NewRule<BinaryOperatorRule<BinaryArithmetic<plus>>>;

constexpr auto Times2 = NewRule<BinaryOperatorRule<BinaryArithmetic<times>>>;

class Plus3Rule : public AtLeastNRule<3> {
public:
	using AtLeastNRule<3>::AtLeastNRule;

	virtual BaseExpressionRef try_apply(
		const ExpressionRef &expr, const Evaluation &evaluation) const;
};

constexpr auto Plus3 = NewRule<Plus3Rule>;

class PowerRule : public ExactlyNRule<2> {
public:
	using ExactlyNRule<2>::ExactlyNRule;

	virtual BaseExpressionRef try_apply(
		const ExpressionRef &expr, const Evaluation &evaluation) const {

		const StaticExpression<2>* expr2 = static_cast<const StaticExpression<2>*>(expr.get());
		const BaseExpressionRef *refs = expr2->_leaves.refs();
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

constexpr auto Less = NewRule<BinaryOperatorRule<BinaryComparison<less>>>;
constexpr auto Greater = NewRule<BinaryOperatorRule<BinaryComparison<greater>>>;

template<typename T>
inline BaseExpressionRef add_only_integers(const T &self) {
	// sums an all MachineInteger/BigInteger expression

	mpint result(0);

	for (auto value : self.template primitives<mpint>()) {
		result += value;
	}

	return from_primitive(std::move(result.to_primitive()));
}

template<typename T>
inline BaseExpressionRef add_only_machine_reals(const T &self) {
	// sums an all MachineReal expression

	machine_real_t result = 0.;

	for (auto value : self.template primitives<machine_real_t>()) {
		result += value;
	}

	return from_primitive(result);
}

template<typename T>
inline BaseExpressionRef add_machine_inexact(const T &self) {
	// create an array to store all the symbolic arguments which can't be evaluated.

	std::vector<BaseExpressionRef> symbolics;
	symbolics.reserve(self.size());

	machine_real_t sum = 0.0;
	for (auto leaf : self.leaves()) {
		auto type = leaf->type();
		switch(type) {
			case MachineIntegerType:
				sum += static_cast<machine_real_t>(
						boost::static_pointer_cast<const MachineInteger>(leaf)->value);
				break;
			case BigIntegerType:
				sum += boost::static_pointer_cast<const BigInteger>(leaf)->value.get_d();
				break;
			case MachineRealType:
				sum += boost::static_pointer_cast<const MachineReal>(leaf)->value;
				break;
			case BigRealType:
				sum += boost::static_pointer_cast<const BigReal>(leaf)->value.toDouble(MPFR_RNDN);
				break;
			case RationalType:
				sum += boost::static_pointer_cast<const Rational>(leaf)->value.get_d();
				break;
			case ComplexType:
				assert(false);
				break;
			case ExpressionType:
			case SymbolType:
			case StringType:
				symbolics.push_back(leaf);
				break;

			default:
				throw std::runtime_error("unsupported types"); // FIXME
		}
	}

	// at least one non-symbolic
	assert(symbolics.size() != self.size());

	BaseExpressionRef result;

	if (symbolics.size() == self.size() - 1) {
		// one non-symbolic: nothing to do
		// result = NULL;
	} else if (!symbolics.empty()) {
		// at least one symbolic
		symbolics.push_back(from_primitive(sum));
		result = expression(self._head, std::move(symbolics));
	} else {
		// no symbolics
		result = from_primitive(sum);
	}

	return result;
}

template<typename T>
BaseExpressionRef ArithmeticOperationsImplementation<T>::Plus() const {
	const T &expr = this->expr();

	// we guarantee that expr.size() >= 3 through the given match_size() in the corresponding
	// Rule.

	constexpr TypeMask int_mask = MakeTypeMask(BigIntegerType) | MakeTypeMask(MachineIntegerType);

	// bit field to determine which types are present
	const TypeMask types_seen = expr.exact_type_mask();

	// expression is all MachineReals
	if (types_seen == MakeTypeMask(MachineRealType)) {
		return add_only_machine_reals(expr);
	}

	// expression is all Integers
	if ((types_seen & int_mask) == types_seen) {
		return add_only_integers(expr);
	}

	// expression contains a Real
	if (types_seen & MakeTypeMask(MachineRealType)) {
		return add_machine_inexact(expr);
	}

	// expression contains an Integer
	/*if (types_seen & int_mask) {
		auto integer_result = expr->operations().add_integers();
		// FIXME return Plus[symbolics__, integer_result]
		return integer_result;
	}*/

	// TODO rational and complex

	// expression is symbolic
	return BaseExpressionRef();
}

#endif //CMATHICS_ARITHMETIC_IMPL_H_H
