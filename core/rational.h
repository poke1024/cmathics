#ifndef RATIONAL_H
#define RATIONAL_H

#include <gmp.h>

#include "types.h"
#include "integer.h"

class Rational : public BaseExpression {
public:
    mpq_class value;

    inline Rational(const mpq_class &new_value) :
        BaseExpression(RationalExtendedType), value(new_value) {
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

    virtual bool same(const BaseExpression &expr) const {
        return false; // FIXME
    }

    virtual std::string fullform() const {
        return "???"; // FIXME
    }

    // copies denominator to a new Integer
    inline BaseExpressionRef numer() const {
        return from_primitive(value.get_num());
    }

    // copies numerator to a new Integer
    inline BaseExpressionRef denom() const {
        return from_primitive(value.get_den());
    };
};

inline BaseExpressionRef from_primitive(const mpq_class &value) {
    return BaseExpressionRef(new Rational(value));
}

#endif
