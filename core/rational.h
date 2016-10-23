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

    // copies denominator to a new Integer
    Integer* numer() const {
        mpz_t x;
        mpz_set(x, mpq_numref(value));
        return Integer_from_mpz(x);
    }


    // copies numerator to a new Integer
    Integer* denom() const {
        mpz_t x;
        mpz_set(x, mpq_denref(value));
        return Integer_from_mpz(x);
    }
};

#endif
