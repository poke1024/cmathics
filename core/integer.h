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

	virtual std::string debugform() const;

    virtual BaseExpressionRef make_boxes(
        BaseExpressionPtr form,
        const Evaluation &evaluation) const;

	virtual std::string boxes_to_text(const Evaluation &evaluation) const;

    virtual BaseExpressionPtr head(const Symbols &symbols) const final;

    virtual inline bool same(const BaseExpression &expr) const final {
        if (expr.is_machine_integer()) {
            return value == static_cast<const MachineInteger*>(&expr)->value;
        } else {
            return false;
        }
    }

    virtual hash_t hash() const {
	    return hash_pair(machine_integer_hash, value);
    }

    virtual inline bool match(const BaseExpression &expr) const final {
        return same(expr);
    }

    virtual double round_to_float() const {
        return value;
    }

	virtual bool is_numeric() const {
		return true;
	}

    virtual bool is_negative() const {
        return value < 0;
    }

    virtual BaseExpressionRef negate(const Evaluation &evaluation) const final;

protected:
	virtual inline SymbolicFormRef instantiate_symbolic_form() const final {
        return Pool::SymbolicForm(SymEngine::integer(value));
	}
};

class BigInteger : public Integer {
public:
	static constexpr Type Type = BigIntegerType;

	mpz_class value;
    mutable optional<hash_t> m_hash;

	inline BigInteger(const mpz_class &new_value) :
		Integer(BigIntegerExtendedType), value(new_value) {
	}

	inline BigInteger(mpz_class &&new_value) :
	    Integer(BigIntegerExtendedType), value(new_value) {
    }

	virtual std::string debugform() const;

    virtual BaseExpressionRef make_boxes(
        BaseExpressionPtr form,
        const Evaluation &evaluation) const;

	virtual std::string boxes_to_text(const Evaluation &evaluation) const;

	virtual BaseExpressionPtr head(const Symbols &symbols) const final;

    virtual inline bool same(const BaseExpression &expr) const final {
        if (expr.is_big_integer()) {
            return value == static_cast<const BigInteger*>(&expr)->value;
        } else {
            return false;
        }
    }

    virtual hash_t hash() const {
        if (!m_hash) {
            m_hash = hash_mpz(value);
        }
        return *m_hash;
    }

    virtual double round_to_float() const {
        return value.get_d();
    }

	virtual bool is_numeric() const {
		return true;
	}

    virtual bool is_negative() const {
        return value < 0;
    }

    virtual BaseExpressionRef negate(const Evaluation &evaluation) const final;

protected:
	virtual inline SymbolicFormRef instantiate_symbolic_form() const final {
		return Pool::SymbolicForm(SymEngine::integer(value.get_mpz_t()));
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

inline bool BaseExpression::is_minus_one() const {
    if (type() == MachineIntegerType) {
        return static_cast<const MachineInteger*>(this)->value == -1;
    } else {
        return false;
    }
}

inline optional<machine_integer_t> BaseExpression::get_int_value() const {
    if (type() == MachineIntegerType) {
        return static_cast<const MachineInteger*>(this)->value;
    } else {
        return optional<machine_integer_t>();
    }
}

#endif
