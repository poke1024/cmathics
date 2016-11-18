#ifndef CMATHICS_ARITHMETIC_IMPL_H_H
#define CMATHICS_ARITHMETIC_IMPL_H_H

#include "arithmetic.h"
#include "integer.h"
#include "real.h"
#include "rational.h"
#include "expression.h"

template<typename T>
inline BaseExpressionRef add_only_integers(const T &self) {
	// sums an all MachineInteger/BigInteger expression

	mpint result(0);

	for (auto value : self.template primitives<mpint>()) {
		result += value;
	}

	return from_primitive(result.to_primitive());
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

	switch (expr.size()) {
		case 0:
			// Plus[] -> 0
			return from_primitive(0LL);

		case 1:
			// Plus[a_] -> a
			return expr.leaf(0);
	}

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

	Function _functions[NumTypes * NumTypes];

	template<typename U, typename V, typename W>
	void init() {
		_functions[U::Type + V::Type * NumTypes] = [] (const BaseExpression *a, const BaseExpression *b) {
			constexpr bool ia = std::is_same<decltype(static_cast<const U*>(a)->value), const W>::value;
			constexpr bool ib = std::is_same<decltype(static_cast<const V*>(b)->value), const W>::value;
			return calculate<F, U, V, W, ia, ib>()(a, b);
		};
	};

	static inline BaseExpressionRef result(const Definitions &definitions, const BaseExpressionRef &result) {
		return result;
	}

	static inline BaseExpressionRef result(const Definitions &definitions, bool result) {
		return definitions.Boolean(result);
	}

public:
	BaseExpressionRef operator()(const Definitions &definitions, const BaseExpressionRef *leaves) const {
		const BaseExpression * const a = leaves[0].get();
		const BaseExpression * const b = leaves[1].get();
		const Function f = _functions[a->type() + b->type() * NumTypes];
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
		BinaryOperator<F>::template init<MachineInteger, MachineInteger, mpz_class>();
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
		return from_primitive(x);
	}
};

class PlusArithmetic : public BinaryArithmetic<plus> {
};

class LessComparison : public BinaryComparison<less> {
};

class GreaterComparison : public BinaryComparison<greater> {
};

extern PlusArithmetic g_plus;
extern LessComparison g_less;
extern GreaterComparison g_greater;

template<typename T, typename C>
inline BaseExpressionRef comparison(const T &expr, const C &comparison, const Evaluation &evaluation) {
	if (expr.size() != 2) {
		return BaseExpressionRef();
	} else {
		const Definitions &definitions = evaluation.definitions;
		const BaseExpressionRef *leaves = expr._leaves.refs();
		return comparison(definitions, leaves);
	}
}

template<typename T>
BaseExpressionRef ArithmeticOperationsImplementation<T>::Less(const Evaluation &evaluation) const {
	return comparison(this->expr(), g_less, evaluation);
}

template<typename T>
BaseExpressionRef ArithmeticOperationsImplementation<T>::Greater(const Evaluation &evaluation) const {
	return comparison(this->expr(), g_greater, evaluation);
}

#endif //CMATHICS_ARITHMETIC_IMPL_H_H
