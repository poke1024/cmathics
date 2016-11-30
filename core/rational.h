#ifndef RATIONAL_H
#define RATIONAL_H

#include <gmp.h>
#include <symengine/rational.h>

#include "types.h"
#include "integer.h"
#include "symbol.h"

class Rational : public BaseExpression {
public:
    static constexpr Type Type = RationalType;

    mpq_class value;

    inline Rational(machine_integer_t x, machine_integer_t y) :
        BaseExpression(RationalExtendedType), value(machine_integer_to_mpz(x), machine_integer_to_mpz(y)) {
    }

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
        std::ostringstream s;
        s << value.get_num().get_str() << " / " << value.get_den().get_str();
        return s.str();
    }

    /*// copies denominator to a new Integer
    inline BaseExpressionRef numer() const {
        return from_primitive(value.get_num());
    }

    // copies numerator to a new Integer
    inline BaseExpressionRef denom() const {
        return from_primitive(value.get_den());
    };*/

protected:
    virtual SymbolicForm instantiate_symbolic_form() const {
        return SymEngine::Rational::from_mpq(value.get_mpq_t());
    }
};

#endif
