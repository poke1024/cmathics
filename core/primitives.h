#ifndef CMATHICS_TO_VALUE_H
#define CMATHICS_TO_VALUE_H

#include "integer.h"
#include "real.h"
#include "rational.h"

class to_primitive_error : public std::runtime_error {
public:
	to_primitive_error(Type type, const char *name) : std::runtime_error(
		std::string("cannot convert ") + type_name(type) + std::string(" to ") + name) {
	}
};

template<typename T>
T to_primitive(const BaseExpressionRef &expr) {
	assert(false);
}

template<>
inline int64_t to_primitive<int64_t>(const BaseExpressionRef &expr) {
	switch (expr->type()) {
		case MachineIntegerType:
			return std::static_pointer_cast<const MachineInteger>(expr)->value;
		default:
			throw to_primitive_error(expr->type(), "int64_t");
	}
}

template<>
inline mpq_class to_primitive<mpq_class>(const BaseExpressionRef &expr) {
	switch (expr->type()) {
		case RationalType:
			return std::static_pointer_cast<const Rational>(expr)->value;
		default:
			throw to_primitive_error(expr->type(), "mpq_class");
	}
}

template<>
inline machine_real_t to_primitive<machine_real_t>(const BaseExpressionRef &expr) {
	switch (expr->type()) {
		case MachineIntegerType:
			return std::static_pointer_cast<const MachineInteger>(expr)->value;
		case BigIntegerType:
			return std::static_pointer_cast<const BigInteger>(expr)->value.get_d();
		case MachineRealType:
			return std::static_pointer_cast<const MachineReal>(expr)->value;
		case BigRealType:
			return std::static_pointer_cast<const BigReal>(expr)->_value.toDouble();
		default:
			throw to_primitive_error(expr->type(), "machine_real_t");
	}
}

template<>
inline mpfr::mpreal to_primitive<mpfr::mpreal>(const BaseExpressionRef &expr) {
	switch (expr->type()) {
		case MachineIntegerType:
			return mpfr::mpreal(std::static_pointer_cast<const MachineInteger>(expr)->value);
		case BigIntegerType:
			return mpfr::mpreal(std::static_pointer_cast<const BigInteger>(expr)->value.get_mpz_t());
		case MachineRealType:
			return mpfr::mpreal(std::static_pointer_cast<const MachineReal>(expr)->value);
		case BigRealType:
			return std::static_pointer_cast<const BigReal>(expr)->_value;
		default:
			throw to_primitive_error(expr->type(), "mpreal");
	}
}

#endif //CMATHICS_TO_VALUE_H
