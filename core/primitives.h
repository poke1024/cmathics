#ifndef CMATHICS_TO_VALUE_H
#define CMATHICS_TO_VALUE_H

#include "integer.h"
#include "real.h"
#include "rational.h"
#include "numeric.h"

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
inline Numeric::Z to_primitive<Numeric::Z>(const BaseExpressionRef &expr) {
	switch (expr->type()) {
		case MachineIntegerType:
			return Numeric::Z(static_cast<const MachineInteger*>(expr.get())->value);
		case BigIntegerType:
			return Numeric::Z(static_cast<const BigInteger*>(expr.get())->value);
		default:
			throw to_primitive_error(expr->type(), "Numeric::Z");
	}
}

template<>
inline int64_t to_primitive<int64_t>(const BaseExpressionRef &expr) {
	switch (expr->type()) {
		case MachineIntegerType:
			return static_cast<const MachineInteger*>(expr.get())->value;
		default:
			throw to_primitive_error(expr->type(), "int64_t");
	}
}

template<>
inline mpq_class to_primitive<mpq_class>(const BaseExpressionRef &expr) {
	switch (expr->type()) {
		case RationalType:
			return static_cast<const Rational*>(expr.get())->value;
		default:
			throw to_primitive_error(expr->type(), "mpq_class");
	}
}

template<>
inline machine_real_t to_primitive<machine_real_t>(const BaseExpressionRef &expr) {
	switch (expr->type()) {
		case MachineIntegerType:
			return static_cast<const MachineInteger*>(expr.get())->value;
		case BigIntegerType:
			return static_cast<const BigInteger*>(expr.get())->value.get_d();
		case MachineRealType:
			return static_cast<const MachineReal*>(expr.get())->value;
		case BigRealType:
			return static_cast<const BigReal*>(expr.get())->as_double();
		default:
			throw to_primitive_error(expr->type(), "machine_real_t");
	}
}

/*template<>
inline mpfr::mpreal to_primitive<mpfr::mpreal>(const BaseExpressionRef &expr) {
	switch (expr->type()) {
		case MachineIntegerType:
			return mpfr::mpreal(static_cast<const MachineInteger*>(expr.get())->value);
		case BigIntegerType:
			return mpfr::mpreal(static_cast<const BigInteger*>(expr.get())->value.get_mpz_t());
		case MachineRealType:
			return mpfr::mpreal(static_cast<const MachineReal*>(expr.get())->value);
		case BigRealType:
			return static_cast<const BigReal*>(expr.get())->value;
		default:
			throw to_primitive_error(expr->type(), "mpreal");
	}
}*/

template<typename T>
class TypeFromPrimitive {
};

template<>
class TypeFromPrimitive<machine_integer_t> {
public:
	static constexpr Type type = MachineIntegerType;
};

template<>
class TypeFromPrimitive<mpz_class> {
public:
	static constexpr Type type = BigIntegerType;
};

template<>
class TypeFromPrimitive<mpq_class> {
public:
	static constexpr Type type = RationalType;
};

template<>
class TypeFromPrimitive<machine_real_t> {
public:
	static constexpr Type type = MachineRealType;
};

template<>
class TypeFromPrimitive<std::string> {
public:
	static constexpr Type type = StringType;
};

#endif //CMATHICS_TO_VALUE_H
