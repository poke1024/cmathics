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

inline BaseExpressionRef from_primitive(machine_integer_t value) {
	return std::make_shared<MachineInteger>(value);
}

inline BaseExpressionRef from_primitive(const mpz_class &value) {
	static_assert(sizeof(long) == sizeof(machine_integer_t), "types long and machine_integer_t differ");
	if (value.fits_slong_p()) {
		return from_primitive(static_cast<machine_integer_t>(value.get_si()));
	} else {
		return std::make_shared<BigInteger>(value);
	}
}

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

    mpint(const std::string &value) {
        throw std::runtime_error("cannot create mpint from std::string");
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

#endif
