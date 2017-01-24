#ifndef INT_H
#define INT_H

#include <gmpxx.h>
#include <stdint.h>
#include <symengine/integer.h>

#include "types.h"
#include "hash.h"

class Integer : public BaseExpression {
public:
	inline Integer(ExtendedType type) : BaseExpression(type) {
	}
};

class MachineInteger : public Integer {
public:
    static constexpr Type Type = MachineIntegerType;

    const machine_integer_t value;

    inline MachineInteger(machine_integer_t new_value) :
	    Integer(MachineIntegerExtendedType), value(new_value) {
    }

    virtual BaseExpressionPtr head(const Evaluation &evaluation) const final;

    virtual inline bool same(const BaseExpression &expr) const final {
        if (expr.type() == type()) {
            return value == static_cast<const MachineInteger*>(&expr)->value;
        } else {
            return false;
        }
    }

    virtual hash_t hash() const {
	    return hash_pair(machine_integer_hash, value);
    }

    virtual std::string format(const SymbolRef &form) const {
        return std::to_string(value);
    }

    virtual inline bool match(const BaseExpression &expr) const final {
        return same(expr);
    }

    virtual double round_to_float() const {
        return value;
    }

protected:
	virtual inline SymbolicFormRef instantiate_symbolic_form() const final {
        return Pool::SymbolicForm(SymEngine::integer(value), true);
	}
};

class BigInteger : public Integer {
public:
	static constexpr Type Type = BigIntegerType;

	mpz_class value;

	inline BigInteger(const mpz_class &new_value) :
		Integer(BigIntegerExtendedType), value(new_value) {
	}

	inline BigInteger(mpz_class &&new_value) :
	    Integer(BigIntegerExtendedType), value(new_value) {
    }

    virtual BaseExpressionPtr head(const Evaluation &evaluation) const final;

    virtual inline bool same(const BaseExpression &expr) const final {
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

    virtual std::string format(const SymbolRef &form) const {
        return value.get_str();
    }

    virtual double round_to_float() const {
        return value.get_d();
    }

protected:
	virtual inline SymbolicFormRef instantiate_symbolic_form() const final {
		return Pool::SymbolicForm(SymEngine::integer(value.get_mpz_t()), true);
	}
};

inline BaseExpressionRef from_primitive(const mpz_class &value);

inline mpz_class machine_integer_to_mpz(machine_integer_t machine_value) {
	mpz_class value;
	value = long(machine_value);
	return value;
}

inline bool BaseExpression::is_zero() const {
    if (type() == MachineIntegerType) {
        return static_cast<const MachineInteger*>(this)->value == 0;
    } else {
        return false;
    }
}

inline bool BaseExpression::is_one() const {
    if (type() == MachineIntegerType) {
        return static_cast<const MachineInteger*>(this)->value == 1;
    } else {
        return false;
    }
}

#endif
