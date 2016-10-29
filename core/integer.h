#ifndef INT_H
#define INT_H

#include <gmp.h>
#include <stdint.h>

#include "types.h"
#include "hash.h"

class Integer : public BaseExpression {
};

class MachineInteger : public Integer {
public:
    const int64_t value;

    inline MachineInteger(int64_t new_value) : value(new_value) {
    }

    virtual Type type() const {
        return MachineIntegerType;
    }

    virtual bool same(const BaseExpression *expr) const {
        if (expr->type() == MachineIntegerType) {
            return value == static_cast<const MachineInteger*>(expr)->value;
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

    virtual Match match(const BaseExpression *expr) const {
        return Match(same(expr));
    }
};

class BigInteger : public Integer {
public:
    mpz_t value;

    inline BigInteger(mpz_srcptr new_value) {
        mpz_init(value);
        mpz_set(value, new_value);
    }

    virtual ~BigInteger() {
        mpz_clear(value);
    }

    virtual Type type() const {
        return BigIntegerType;
    }

    virtual bool same(const BaseExpression *expr) const {
        if (expr->type() == BigIntegerType) {
            return mpz_cmp(value, static_cast<const BigInteger*>(expr)->value) == 0;
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

inline BaseExpressionRef integer(int64_t value) {
    return std::make_shared<MachineInteger>(value);
}

inline BaseExpressionRef integer(mpz_srcptr value) {
    if (mpz_fits_slong_p(value)) {
        return integer(mpz_get_si(value));
    } else {
        return std::make_shared<BigInteger>(value);
    }
}

#endif
