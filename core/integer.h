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

class mpint {
public:
    long machine_value;
    mpz_t big_value;
    bool is_big;

    static_assert(sizeof(machine_integer_t) == sizeof(machine_value),
        "machine integer type must equivalent to long");

    inline mpint(machine_integer_t value = 0) : machine_value(value), is_big(false) {
    }

    inline mpint(mpint &&x) {
        if (x.is_big) {
            is_big = true;
            std::memcpy(&big_value, &x.big_value, sizeof(mpz_t));
            x.is_big = false;
        } else {
            machine_value = x.machine_value;
        }
    }

    inline mpint(const mpint &x) {
        if (x.is_big) {
            is_big = true;
            mpz_init_set(big_value, x.big_value);
        } else {
            machine_value = x.machine_value;
        }
    }

    inline mpint(const mpz_class &value) : is_big(true) {
        mpz_init_set(big_value, value.get_mpz_t());
    }

    mpint(const mpq_class &value) {
        throw std::runtime_error("cannot create mpint from mpq_class");
    }

    mpint(const std::string &value) {
        throw std::runtime_error("cannot create mpint from std::string");
    }

    ~mpint() {
        if (is_big) {
            mpz_clear(big_value);
        }
    }

    mpint operator+(const mpint &y) const {
        mpint x(*this);
        x += y;
        return x;
    }

    mpint &operator+=(const mpint &other) {
        if (!is_big) {
            if (!other.is_big) {
                long r;
                if (!__builtin_saddl_overflow(machine_value, other.machine_value, &r)) {
                    machine_value = r;
                    return *this;
                }
            }
            mpz_init_set_si(big_value, machine_value);
            is_big = true;
        }
        if (other.is_big) {
            mpz_add(big_value, big_value, other.big_value);
            return *this;
        } else {
            const auto value = other.machine_value;
            if (value >= 0) {
                mpz_add_ui(big_value, big_value, (unsigned long)value);
            } else {
                mpz_sub_ui(big_value, big_value, (unsigned long)-value);
            }
            return *this;
        }
    }

    mpz_class to_primitive() const {
        if (!is_big) {
            mpz_class value;
            value = machine_value;
            return value;
        } else {
            return mpz_class(big_value);
        }
    }
};

#endif
