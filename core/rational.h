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
    mutable optional<hash_t> m_hash;

    inline BigRational(machine_integer_t x, machine_integer_t y) :
        BaseExpression(BigRationalExtendedType),
        value(machine_integer_to_mpz(x), machine_integer_to_mpz(y)) {
    }

    inline BigRational(const mpq_class &new_value) :
        BaseExpression(BigRationalExtendedType), value(new_value) {
    }

    virtual BaseExpressionRef make_boxes(
        BaseExpressionPtr form,
        const Evaluation &evaluation) const;

    virtual BaseExpressionPtr head(const Evaluation &evaluation) const final;

    virtual hash_t hash() const {
        if (!m_hash) {
            m_hash = hash_combine(hash_mpz(value.get_num()), hash_mpz(value.get_den()));
        }
        return *m_hash;
    }

    virtual inline bool same(const BaseExpression &expr) const final {
        if (expr.type() == BigRationalType) {
	        const auto other = static_cast<const BigRational*>(&expr);
	        return mpq_equal(value.get_mpq_t(), other->value.get_mpq_t());
        } else {
	        return false;
        }
    }

    virtual std::string format(const SymbolRef &form, const Evaluation &evaluation) const final;

    /*// copies denominator to a new Integer
    inline BaseExpressionRef numer() const {
        return from_primitive(value.get_num());
    }

    // copies numerator to a new Integer
    inline BaseExpressionRef denom() const {
        return from_primitive(value.get_den());
    };*/

    virtual double round_to_float() const {
        return value.get_d();
    }

    virtual bool is_numeric() const {
        return true;
    }

protected:
    virtual inline SymbolicFormRef instantiate_symbolic_form() const final {
        return Pool::SymbolicForm(SymEngine::Rational::from_mpq(value.get_mpq_t()));
    }
};

#endif
