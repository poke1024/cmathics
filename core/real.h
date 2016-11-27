#ifndef REAL_H
#define REAL_H

#include <mpfrcxx/mpreal.h>
#include <stdint.h>
#include <sstream>

#include "types.h"
#include "hash.h"

class MachineReal : public BaseExpression {
public:
	static constexpr Type Type = MachineRealType;

    const machine_real_t value;

    explicit MachineReal(machine_real_t new_value) :
	    BaseExpression(MachineRealExtendedType), value(new_value) {
    }

    virtual bool same(const BaseExpression &expr) const {
        if (expr.type() == MachineRealType) {
            return value == static_cast<const MachineReal*>(&expr)->value;
        } else {
            return false;
        }
    }

    virtual hash_t hash() const {
        // TODO better hash see CPython _Py_HashDouble
        return hash_pair(machine_real_hash, (uint64_t)value);
    }

    virtual std::string fullform() const {
        return std::to_string(value); // FIXME
    }

    virtual bool match(const BaseExpression &expr) const {
        return same(expr);
    }
};

inline mp_prec_t bits_prec(double prec) {
	constexpr double LOG_2_10 = 3.321928094887362; // log2(10.0);
	return static_cast<mp_prec_t>(ceil(LOG_2_10 * prec));
}

inline mp_prec_t from_bits_prec(mp_prec_t bits_prec) {
	constexpr double LOG_2_10 = 3.321928094887362; // log2(10.0);
	return bits_prec / LOG_2_10;
}

class BigReal : public BaseExpression {
public:
	static constexpr Type Type = BigRealType;

    mpfr::mpreal value;
	const double prec;

	explicit inline BigReal(double new_prec, double new_value) :
		BaseExpression(BigRealExtendedType), value(new_value, bits_prec(new_prec), MPFR_RNDN), prec(new_prec) {
	}

	explicit inline BigReal(const mpfr::mpreal &new_value) :
		BaseExpression(BigRealExtendedType), value(new_value), prec(from_bits_prec(value.get_prec())) {
	}

	virtual bool same(const BaseExpression &expr) const {
        if (expr.type() == BigRealType) {
            return value == static_cast<const BigReal*>(&expr)->value;
        } else {
            return false;
        }
    }

    virtual hash_t hash() const {
        // TODO hash
        return 0;
    }

    virtual std::string fullform() const {
	    std::stringstream s; // FIXME
	    s << value;
        return s.str();
    }
};

inline BaseExpressionRef from_primitive(machine_real_t value) {
	return BaseExpressionRef(new MachineReal(value));
}

inline BaseExpressionRef from_primitive(const mpfr::mpreal &value) {
	return BaseExpressionRef(new BigReal(value));
}

inline BaseExpressionRef real(double prec, machine_real_t value) {
	return BaseExpressionRef(new BigReal(prec, value));
}

std::pair<int32_t,double> precision_of(const BaseExpressionRef&);

#endif
