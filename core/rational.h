#ifndef RATIONAL_H
#define RATIONAL_H

#include <gmp.h>
#include <symengine/rational.h>

#include "types.h"
#include "integer.h"
#include "symbol.h"

class BigRational : public BaseExpression {
public:
    static constexpr Type Type = BigRationalType;

    mpq_class value;

    inline BigRational(machine_integer_t x, machine_integer_t y) :
        BaseExpression(BigRationalExtendedType),
        value(machine_integer_to_mpz(x), machine_integer_to_mpz(y)) {
    }

    inline BigRational(const mpq_class &new_value) :
        BaseExpression(BigRationalExtendedType), value(new_value) {
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
    virtual bool instantiate_symbolic_form() const {
        set_symbolic_form(SymEngine::Rational::from_mpq(value.get_mpq_t()));
        return true;
    }
};

#endif
