#ifndef INT_H
#define INT_H

#include <gmpxx.h>
#include <stdint.h>

#include "types.h"
#include "hash.h"

class Integer : public BaseExpression {
public:
	inline Integer(Type type) : BaseExpression(type) {
	}
};

class MachineInteger : public Integer {
public:
    const machine_integer_t value;

    inline MachineInteger(machine_integer_t new_value) :
	    Integer(MachineIntegerType), value(new_value) {
    }

    virtual bool same(const BaseExpression &expr) const {
        if (expr.type() == type()) {
            return value == static_cast<const MachineInteger*>(&expr)->value;
        } else {
            return false;
        }
    }

    virtual hash_t hash() const {
        return hash_pair(machine_integer_hash, value);
    }

    virtual std::string fullform() const {
        return std::to_string(value);
    }

    virtual bool match(const BaseExpression &expr) const {
        return same(expr);
    }
};

class BigInteger : public Integer {
public:
	mpz_class value;

    inline BigInteger(const mpz_class &new_value) : Integer(BigIntegerType), value(new_value) {
    }

    virtual bool same(const BaseExpression &expr) const {
        if (expr.type() == BigIntegerType) {
            return value == static_cast<const BigInteger*>(&expr)->value;
        } else {
            return false;
        }
    }

    virtual hash_t hash() const {
        //  TODO hash
        return 0;
    }

    virtual std::string fullform() const {
        return std::string("biginteger");  // FIXME
    }
};

inline BaseExpressionRef integer(machine_integer_t value) {
    return std::make_shared<MachineInteger>(value);
}

inline BaseExpressionRef integer(const mpz_class &value) {
    static_assert(sizeof(long) == sizeof(machine_integer_t), "types long and machine_integer_t differ");
    if (value.fits_slong_p()) {
        return integer(value.get_si());
    } else {
        return std::make_shared<BigInteger>(value);
    }
}

#endif
