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

    virtual bool same(const BaseExpression &expr) const {
        if (expr.type() == type()) {
            return value == static_cast<const MachineInteger*>(&expr)->value;
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

    virtual bool match(const BaseExpression &expr) const {
        return same(expr);
    }

protected:
	virtual SymbolicForm instantiate_symbolic_form() const {
		return SymEngine::integer(value);
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

    virtual bool same(const BaseExpression &expr) const {
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

    virtual std::string fullform() const {
        return std::string("biginteger");  // FIXME
    }

protected:
	virtual SymbolicForm instantiate_symbolic_form() const {
		return SymEngine::integer(value.get_mpz_t());
	}
};

class mpint {
private:
	template<bool c_is_a>
	inline static void add(mpint &c, const mpint &a, const mpint &b) {
		if (!a.is_big) {
			if (!b.is_big) {
				long r;
				if (!__builtin_saddl_overflow(a.machine_value, b.machine_value, &r)) {
					if (!c_is_a) {
						c.is_big = false;
					}
					c.machine_value = r;
					return;
				}
			}
			mpz_init_set_si(c.big_value, a.machine_value);
			c.is_big = true;
		}
		if (b.is_big) {
			mpz_add(c.big_value, c.big_value, b.big_value);
		} else {
			const auto value = b.machine_value;
			if (value >= 0) {
				mpz_add_ui(c.big_value, c.big_value, (unsigned long)value);
			} else {
				mpz_sub_ui(c.big_value, c.big_value, (unsigned long)-value);
			}
		}
	}

	template<bool c_is_a>
	inline static void mul(mpint &c, const mpint &a, const mpint &b) {
		if (!a.is_big) {
			/*if (!b.is_big) {
				long r;
				if (!__builtin_saddl_overflow(a.machine_value, b.machine_value, &r)) {
					if (!c_is_a) {
						c.is_big = false;
					}
					c.machine_value = r;
					return;
				}
			}*/
			mpz_init_set_si(c.big_value, a.machine_value);
			c.is_big = true;
		}
		if (b.is_big) {
			mpz_mul(c.big_value, c.big_value, b.big_value);
		} else {
			mpz_mul_si(c.big_value, c.big_value, b.machine_value);
		}
	}

	inline mpint() {
	}

public:
    long machine_value;
    mpz_t big_value;
    bool is_big;

    static_assert(sizeof(machine_integer_t) == sizeof(decltype(machine_value)),
        "machine integer type must equivalent to long");

    inline explicit mpint(machine_integer_t value) : machine_value(value), is_big(false) {
    }

    inline mpint(mpint &&x) {
        if (x.is_big) {
            is_big = true;
            std::memcpy(&big_value, &x.big_value, sizeof(mpz_t));
            x.is_big = false;
        } else {
	        is_big = false;
            machine_value = x.machine_value;
        }
    }

    inline mpint(const mpint &x) {
        if (x.is_big) {
            is_big = true;
            mpz_init_set(big_value, x.big_value);
        } else {
	        is_big = false;
            machine_value = x.machine_value;
        }
    }

    inline mpint(const mpz_class &value) : is_big(true) {
        mpz_init_set(big_value, value.get_mpz_t());
    }

    mpint(const mpq_class &value) {
        throw std::runtime_error("cannot create mpint from mpq_class");
    }

    mpint(const std::string &value) {
        throw std::runtime_error("cannot create mpint from std::string");
    }

    ~mpint() {
        if (is_big) {
            mpz_clear(big_value);
        }
    }

    inline mpint operator+(const mpint &other) const {
	    mpint x;
	    add<false>(x, *this, other);
        return x;
    }

	inline mpint &operator+=(const mpint &other) {
	    add<true>(*this, *this, other);
	    return *this;
    }

	inline mpint operator*(const mpint &other) const {
		mpint x;
		mul<false>(x, *this, other);
		return x;
	}

	inline mpint &operator*=(const mpint &other) {
		mul<true>(*this, *this, other);
		return *this;
	}

    mpz_class to_primitive() const {
        if (!is_big) {
            mpz_class value;
            value = machine_value;
            return value;
        } else {
            return mpz_class(big_value);
        }
    }
};

inline BaseExpressionRef from_primitive(const mpint &value) {
	if (value.is_big) {
		return from_primitive(std::move(mpz_class(value.big_value)));
	} else {
		return from_primitive(machine_integer_t(value.machine_value));
	}
}

#endif
