#ifndef CMATHICS_ARITHMETIC_IMPL_H_H
#define CMATHICS_ARITHMETIC_IMPL_H_H

#include "arithmetic.h"
#include "integer.h"
#include "real.h"
#include "rational.h"
#include "expression.h"
#include "numeric.h"

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
		BinaryOperator<F>::template init<MachineInteger, MachineReal>();
		// BinaryOperator<F>::template init<MachineInteger, BigReal>();
        BinaryOperator<F>::template init<MachineInteger, Rational>();

		BinaryOperator<F>::template init<BigInteger, MachineInteger>();
		BinaryOperator<F>::template init<BigInteger, BigInteger>();
		// BinaryOperator<F>::template init<BigInteger, MachineReal>();
		// BinaryOperator<F>::template init<BigInteger, BigReal>();
        BinaryOperator<F>::template init<BigInteger, Rational>();

		BinaryOperator<F>::template init<MachineReal, MachineInteger>();
		// BinaryOperator<F>::template init<MachineReal, BigInteger>();
		BinaryOperator<F>::template init<MachineReal, MachineReal>();
		// BinaryOperator<F>::template init<MachineReal, BigReal>();
        BinaryOperator<F>::template init<MachineReal, Rational>();

		// BinaryOperator<F>::template init<BigReal, MachineInteger>();
		// BinaryOperator<F>::template init<BigReal, BigInteger>();
		// BinaryOperator<F>::template init<BigReal, MachineReal>();
		// BinaryOperator<F>::template init<BigReal, BigReal>();
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

template<typename T>
RuleRef NewRule(const SymbolRef &head, const Definitions &definitions) {
	return std::make_shared<T>(head, definitions);
}

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

template<typename T>
inline BaseExpressionRef add_only_integers(const T &self) {
	// sums an all MachineInteger/BigInteger expression

	Numeric::Z result(0);

	for (auto value : self.template primitives<Numeric::Z>()) {
		result += value;
	}

    return result.to_expression();
}

template<typename T>
inline BaseExpressionRef add_only_machine_reals(const T &self) {
	// sums an all MachineReal expression

	machine_real_t result = 0.;

	for (machine_real_t value : self.template primitives<machine_real_t>()) {
		result += value;
	}

	return Heap::MachineReal(result);
}

template<typename T>
inline BaseExpressionRef add_machine_inexact(const T &self) {
	// create an array to store all the symbolic arguments which can't be evaluated.

	std::vector<BaseExpressionRef> symbolics;
	symbolics.reserve(self.size());

	machine_real_t sum = 0.0;
	for (const BaseExpressionRef leaf : self.leaves()) {
		const BaseExpression *leaf_ptr = leaf.get();

		const auto type = leaf_ptr->type();
		switch(type) {
			case MachineIntegerType:
				sum += machine_real_t(static_cast<const MachineInteger*>(leaf_ptr)->value);
				break;
			case BigIntegerType:
				sum += static_cast<const BigInteger*>(leaf_ptr)->value.get_d();
				break;
			case MachineRealType:
				sum += static_cast<const MachineReal*>(leaf_ptr)->value;
				break;
			case BigRealType:
				sum += static_cast<const BigReal*>(leaf_ptr)->as_double();
				break;
			case RationalType:
				sum += static_cast<const Rational*>(leaf_ptr)->value.get_d();
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
