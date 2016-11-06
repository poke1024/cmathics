#ifndef REAL_H
#define REAL_H
#include <mpfrcxx/mpreal.h>
#include <stdint.h>

#include "types.h"
#include "hash.h"

class MachineReal : public BaseExpression {
public:
    const machine_real_t value;

    explicit MachineReal(machine_real_t new_value) : BaseExpression(MachineRealType), value(new_value) {
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

class BigReal : public BaseExpression {
public:
    mpfr::mpreal _value;
	const double _prec;

	explicit inline BigReal(double prec, double value) :
		BaseExpression(BigRealType), _value(value, bits_prec(prec), MPFR_RNDN), _prec(prec) {
	}

    virtual bool same(const BaseExpression &expr) const {
        if (expr.type() == BigRealType) {
            return _value == static_cast<const BigReal*>(&expr)->_value;
        } else {
            return false;
        }
    }

    virtual hash_t hash() const {
        // TODO hash
        return 0;
    }

    virtual std::string fullform() const {
        return std::string("bigreal"); // FIXME
    }
};

inline BaseExpressionRef real(double value) {
    return std::make_shared<MachineReal>(value);
}

inline BaseExpressionRef real(double prec, double value) {
    return std::make_shared<BigReal>(prec, value);
}

std::pair<int32_t,double> precision_of(const BaseExpressionRef&);

#endif
