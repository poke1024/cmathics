#ifndef RATIONAL_H
#define RATIONAL_H

#include <gmp.h>
#include <symengine/rational.h>

#include "core/types.h"
#include "integer.h"
#include "symbol.h"

class BigRational : public BaseExpression, public PoolObject<BigRational> {
public:
    static constexpr Type Type = BigRationalType;

    mpq_class value;
    mutable Spinlocked<optional<hash_t>> m_hash;

    inline explicit BigRational(machine_integer_t x, machine_integer_t y) :
        BaseExpression(BigRationalExtendedType),
        value(machine_integer_to_mpz(x), machine_integer_to_mpz(y)) {
    }

    inline explicit BigRational(const mpq_class &new_value) :
        BaseExpression(BigRationalExtendedType), value(new_value) {
    }

	virtual std::string debugform() const;

	virtual BaseExpressionRef custom_format(
		const BaseExpressionRef &form,
		const Evaluation &evaluation) const final;

    virtual BaseExpressionPtr head(const Symbols &symbols) const final;

    virtual hash_t hash() const {
	    return m_hash.lock([this] (auto &hash) {
		    if (!hash) {
			    hash = hash_combine(
					hash_mpz(value.get_num()),
					hash_mpz(value.get_den()));
		    }
		    return *hash;
	    });
    }

    virtual inline bool same_indeed(const BaseExpression &expr) const final {
        if (expr.is_big_rational()) {
	        const auto other = static_cast<const BigRational*>(&expr);
	        return mpq_equal(value.get_mpq_t(), other->value.get_mpq_t());
        } else {
	        return false;
        }
    }

    inline bool is_numerator_one() const {
        return value.get_num() == 1;
    }

    inline BaseExpressionRef numerator() const {
        return from_primitive(value.get_num());
    }

    inline BaseExpressionRef denominator() const {
        return from_primitive(value.get_den());
    };

    virtual double round_to_float() const {
        return value.get_d();
    }

    virtual bool is_numeric() const {
        return true;
    }

    virtual bool is_negative() const {
        return value < 0;
    }

	virtual bool is_positive() const {
		return value > 0;
	}

    virtual BaseExpressionRef negate(const Evaluation &evaluation) const final;

	virtual void sort_key(SortKey &key, const Evaluation &evaluation) const final;

protected:
    virtual inline SymbolicFormRef instantiate_symbolic_form(const Evaluation &evaluation) const final {
        return SymbolicForm::construct(SymEngine::Rational::from_mpq(value.get_mpq_t()));
    }
};

class MachineRational : public BigRational { // for now
public:
    using BigRational::BigRational;
};

#endif
