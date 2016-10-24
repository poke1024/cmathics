#ifndef RATIONAL_H
#define RATIONAL_H

#include <gmp.h>

#include "types.h"
#include "integer.h"

class Rational : public BaseExpression {
public:
    mpq_t value;

    inline Rational(mpq_t new_value) {
        mpq_init(value);
        mpq_set(value, new_value);
    }

    virtual ~Rational() {
        mpq_clear(value);
    }

    virtual Type type() const {
        return RationalType;
    }

    virtual hash_t hash() const {
        /*uint64_t seed = 0;

        numer = (BaseExpression*) Rational_numer(expression);
        denom = (BaseExpression*) Rational_denom(expression);

        seed = hash_combine(seed, rational_hash);
        seed = hash_combine(seed, Hash(numer));
        seed = hash_combine(seed, Hash(denom));*/

        return 0;
    }

    virtual bool same(const BaseExpression *expr) const {
        return false; // FIXME
    }

    virtual std::string fullform() const {
        return "???"; // FIXME
    }

    // copies denominator to a new Integer
    inline Integer* numer() const {
        return Integer_from_mpz(mpq_numref(value));
    }

    // copies numerator to a new Integer
    inline Integer* denom() const {
        return Integer_from_mpz(mpq_denref(value));
    }
};

#endif
