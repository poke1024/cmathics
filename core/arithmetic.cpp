#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <vector>

#include "types.h"
#include "expression.h"
#include "pattern.h"
#include "integer.h"
#include "arithmetic.h"
#include "real.h"
#include "rational.h"
#include "primitives.h"


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

	Function _functions[1 << (2 * CoreTypeBits)];

	template<typename U, typename V, typename W>
	void init() {
		_functions[U::Type + (V::Type << CoreTypeBits)] = [] (const BaseExpression *a, const BaseExpression *b) {
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
	BinaryOperator() {
		std::memset(_functions, 0, sizeof(_functions));
	}

	inline BaseExpressionRef operator()(const Definitions &definitions, const BaseExpressionRef *leaves) const {
		const BaseExpression * const a = leaves[0].get();
		const BaseExpression * const b = leaves[1].get();
		const Function f = _functions[a->type() | (b->type() << CoreTypeBits)];
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

PlusArithmetic g_plus;
LessComparison g_less;
GreaterComparison g_greater;

BaseExpressionRef Plus2::try_apply(
	const ExpressionRef &expr, const Evaluation &evaluation) const {
	const StaticExpression<2>* expr2 = static_cast<const StaticExpression<2>*>(expr.get());
	return g_plus(evaluation.definitions, expr2->_leaves.refs());
}

BaseExpressionRef Plus3::try_apply(
	const ExpressionRef &expr, const Evaluation &evaluation) const {
	return expr->Plus();
}

BaseExpressionRef Less::try_apply(
	const ExpressionRef &expr, const Evaluation &evaluation) const {
	const StaticExpression<2>* expr2 = static_cast<const StaticExpression<2>*>(expr.get());
	return g_less(evaluation.definitions, expr2->_leaves.refs());
}

BaseExpressionRef Greater::try_apply(
	const ExpressionRef &expr, const Evaluation &evaluation) const {
	const StaticExpression<2>* expr2 = static_cast<const StaticExpression<2>*>(expr.get());
	return g_greater(evaluation.definitions, expr2->_leaves.refs());
}

template<typename F>
BaseExpressionRef compute(TypeMask mask, const F &f) {
	// expression contains a Real
	if (mask & MakeTypeMask(MachineRealType)) {
		return f.template compute<double>();
	}

	constexpr TypeMask machine_int_mask = MakeTypeMask(MachineIntegerType);
	// expression is all MachineIntegers
	if ((mask & machine_int_mask) == mask) {
		return f.template compute<int64_t>();
	}

	constexpr TypeMask int_mask = MakeTypeMask(BigIntegerType) | MakeTypeMask(MachineIntegerType);
	// expression is all Integers
	if ((mask & int_mask) == mask) {
		return f.template compute<mpz_class>();
	}

	constexpr TypeMask rational_mask = MakeTypeMask(RationalType);
	// expression is all Rationals
	if ((mask & rational_mask) == mask) {
		return f.template compute<mpq_class>();
	}

	return BaseExpressionRef(); // cannot evaluate
}

class RangeComputation {
private:
	const BaseExpressionRef &_imin;
	const BaseExpressionRef &_imax;
	const BaseExpressionRef &_di;
	const Evaluation &_evaluation;

public:
	inline RangeComputation(
		const BaseExpressionRef &imin,
		const BaseExpressionRef &imax,
		const BaseExpressionRef &di,
		const Evaluation &evaluation) : _imin(imin), _imax(imax), _di(di), _evaluation(evaluation) {
	}

	template<typename T>
	BaseExpressionRef compute() const {
		const T imin = to_primitive<T>(_imin);
		const T imax = to_primitive<T>(_imax);
		const T di = to_primitive<T>(_di);

		std::vector<T> leaves;
		for (T x = imin; x <= imax; x += di) {
			leaves.push_back(x);
		}

		return expression(_evaluation.definitions.List(), PackedSlice<T>(std::move(leaves)));
	}
};

BaseExpressionRef Range(
	const BaseExpressionRef &imin,
    const BaseExpressionRef &imax,
    const BaseExpressionRef &di,
	const Evaluation &evaluation) {
	return compute(
		imin->base_type_mask() | imax->base_type_mask() | di->base_type_mask(),
		RangeComputation(imin, imax, di, evaluation));
}
