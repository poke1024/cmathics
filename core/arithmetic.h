#ifndef ARITHMETIC_H
#define ARITHMETIC_H

#include <experimental/optional>

#include "types.h"
#include "operations.h"

BaseExpressionRef Plus(
	const ExpressionRef &expr,
	const Evaluation &evaluation);

BaseExpressionRef Range(
	const BaseExpressionRef &imin,
	const BaseExpressionRef &imax,
	const BaseExpressionRef &di,
	const Evaluation &evaluation);

class mpint {
public:
	long machine_value;
	mpz_class *big_value;

	mpint(machine_integer_t value) : machine_value(value), big_value(nullptr) {
		static_assert(sizeof(machine_integer_t) == sizeof(machine_value),
			"machine integer type must equivalent to long");
	}

	mpint(const mpz_class &value) {
		big_value = new mpz_class(value);
	}

	mpint(const mpq_class &value) {
		throw std::runtime_error("cannot create mpint from mpq_class");
	}

	~mpint() {
		delete big_value;
	}

	mpint &operator+=(const mpint &other) {
		if (!big_value) {
			if (!other.big_value) {
				long r;
				if (!__builtin_saddl_overflow(machine_value, other.machine_value, &r)) {
					machine_value = r;
					return *this;
				}
			}
			big_value = new mpz_class();
			*big_value = machine_value;
		}
		if (other.big_value) {
			*big_value += *other.big_value;
			return *this;
		} else {
			auto value = other.machine_value;
			*big_value += value;
			return *this;
		}
	}

	mpz_class to_primitive() const {
		if (!big_value) {
			mpz_class value;
			value = machine_value;
			return value;
		} else {
			return *big_value;
		}
	}
};

class ArithmeticOperations {
public:
	virtual BaseExpressionRef add_integers() const = 0;
};

template<typename T>
class ArithmeticOperationsImplementation :
	virtual public ArithmeticOperations,
	virtual public OperationsImplementation<T> {
public:
	virtual BaseExpressionRef add_integers() const {
		// sums all integers in an expression

		mpint result(0);

		// XXX right now the entire computation is done with GMP. This is slower
		// than using machine precision arithmetic but simpler because we don't
		// need to handle overflow. Could be optimised.

		for (auto value : this->template primitives<mpint>()) {
			result += value;
		}

		return from_primitive(result.to_primitive());
	}
};

#endif
