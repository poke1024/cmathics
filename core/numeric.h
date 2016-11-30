#ifndef CMATHICS_NUMERIC_H
#define CMATHICS_NUMERIC_H

#include <gmpxx.h>
#include <arb.h>

#include "types.h"
#include "integer.h"
#include "real.h"
#include "rational.h"
#include "heap.h"

static_assert(sizeof(machine_integer_t) == sizeof(long),
	"machine integer type must equivalent to long");

template<typename U, typename V>
struct Comparison {
	template<typename F>
	inline static bool compare(const U &u, const V &v, const F &f) {
		return f(u.value, v.value);
	}
};

template<>
struct Comparison<BigInteger, MachineInteger> {
	template<typename F>
	inline static bool compare(const BigInteger &u, const MachineInteger &v, const F &f) {
		return f(u.value, long(v.value));
	}
};

template<>
struct Comparison<MachineInteger, BigInteger> {
	template<typename F>
	inline static bool compare(const MachineInteger &u, const BigInteger &v, const F &f) {
		return f(long(u.value), v.value);
	}
};

template<>
struct Comparison<Rational, MachineInteger> {
	template<typename F>
	inline static bool compare(const Rational &u, const MachineInteger &v, const F &f) {
		return f(u.value, long(v.value));
	}
};

template<>
struct Comparison<MachineInteger, Rational> {
	template<typename F>
	inline static bool compare(const MachineInteger &u, const Rational &v, const F &f) {
		return f(long(u.value), v.value);
	}
};

inline BaseExpressionRef operator+(const MachineInteger &x, const MachineInteger &y) {
	long r;
	if (!__builtin_saddl_overflow(x.value, y.value, &r)) {
		return Heap::MachineInteger(r);
	} else {
		return Heap::BigInteger(mpz_class(long(x.value)) + long(y.value));
	}
}

inline BaseExpressionRef operator*(const MachineInteger &x, const MachineInteger &y) {
	long r;
	if (!__builtin_smull_overflow(x.value, y.value, &r)) {
		return Heap::MachineInteger(r);
	} else {
		return Heap::BigInteger(mpz_class(long(x.value)) * long(y.value));
	}
}

inline BaseExpressionRef operator+(const MachineReal &x, const MachineReal &y) {
	return Heap::MachineReal(x.value + y.value);
}

inline BaseExpressionRef operator+(const BigInteger &x, const BigInteger &y) {
	return Heap::BigInteger(x.value + y.value);
}

inline BaseExpressionRef operator+(const BigInteger &x, const MachineInteger &y) {
	return Heap::BigInteger(x.value + long(y.value));
}

inline BaseExpressionRef operator+(const MachineInteger &x, const BigInteger &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const BigInteger &x, const MachineReal &y) {
    return Heap::MachineReal(x.value.get_d() + y.value);
}

inline BaseExpressionRef operator+(const MachineReal &x, const BigInteger &y) {
    return y + x;
}

inline BaseExpressionRef operator+(const BigInteger &x, const BigReal &y) {
    arf_t temp;
    arf_init(temp);
    arf_set_mpz(temp, x.value.get_mpz_t());

    arb_t r;
    arb_init(r);
    arb_add_arf(r, y.value, temp, y.prec.bits);

    arf_clear(temp);

    return Heap::BigReal(r, y.prec);
}

inline BaseExpressionRef operator+(const BigReal &x, const BigInteger &y) {
    return y + x;
}

inline BaseExpressionRef operator+(const Rational &x, const BigInteger &y) {
	return Heap::Rational(x.value + y.value);
}

inline BaseExpressionRef operator+(const BigInteger &x, const Rational &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const Rational &x, const MachineInteger &y) {
	return Heap::Rational(x.value + long(y.value));
}

inline BaseExpressionRef operator+(const MachineInteger &x, const Rational &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const MachineReal &x, const Rational &y) {
	return Heap::MachineReal(x.value + y.value.get_d());
}

inline BaseExpressionRef operator+(const Rational &x, const MachineReal &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const MachineReal &x, const MachineInteger &y) {
	return Heap::MachineReal(x.value + y.value);
}

inline BaseExpressionRef operator+(const MachineInteger &x, const MachineReal &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const MachineInteger &x, const BigReal &y) {
	arb_t r;
	arb_init(r);
	arb_add_si(r, y.value, long(x.value), y.prec.bits);
	return Heap::BigReal(r, y.prec);
}

inline BaseExpressionRef operator+(const BigReal &x, const MachineInteger &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const MachineReal &x, const BigReal &y) {
    return Heap::MachineReal(x.value + y.as_double());
}

inline BaseExpressionRef operator+(const BigReal &x, const MachineReal &y) {
    return y + x;
}

inline BaseExpressionRef operator+(const BigReal &x, const BigReal &y) {
    arb_t r;
    arb_init(r);
    arb_add(r, x.value, y.value, std::min(x.prec.bits, y.prec.bits));
    return Heap::BigReal(r, x.prec.bits < y.prec.bits ? x.prec : y.prec);
}

inline BaseExpressionRef operator+(const Rational &x, const BigReal &y) {
	arf_t num;
	arf_init(num);
	arf_set_mpz(num, x.value.get_num().get_mpz_t());

	arf_t den;
	arf_init(den);
	arf_set_mpz(den, x.value.get_den().get_mpz_t());

	arf_t q;
	arf_init(q);
	arf_div(q, num, den, y.prec.bits, ARF_RND_DOWN);

	arb_t r;
	arb_init(r);
	arb_add_arf(r, y.value, q, y.prec.bits);

	arf_clear(num);
	arf_clear(den);
	arf_clear(q);

	return Heap::BigReal(r, y.prec);
}

inline BaseExpressionRef operator+(const BigReal &x, const Rational &y) {
	return y + x;
}

inline BaseExpressionRef operator+(const Rational &x, const Rational &y) {
	return Heap::Rational(x.value + y.value);
}

inline BaseExpressionRef operator*(const BigInteger &x, const BigInteger &y) {
	return Heap::BigInteger(x.value * y.value);
}

inline BaseExpressionRef operator*(const BigInteger &x, const MachineInteger &y) {
	return Heap::BigInteger(x.value * long(y.value));
}

inline BaseExpressionRef operator*(const MachineInteger &x, const BigInteger &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const Rational &x, const MachineInteger &y) {
	const mpq_class q(x.value * long(y.value));
	if (q.get_den() == 1) {
		return from_primitive(q.get_num());
	} else {
		return Heap::Rational(q);
	}
}

inline BaseExpressionRef operator*(const MachineInteger &x, const Rational &y) {
	return y + x;
}

inline BaseExpressionRef operator*(const Rational &x, const BigInteger &y) {
	const mpq_class q(x.value * y.value);
	if (q.get_den() == 1) {
		return from_primitive(q.get_num());
	} else {
		return Heap::Rational(q);
	}
}

inline BaseExpressionRef operator*(const BigInteger &x, const Rational &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const MachineReal &x, const Rational &y) {
	return Heap::MachineReal(x.value * y.value.get_d());
}

inline BaseExpressionRef operator*(const Rational &x, const MachineReal &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const MachineReal &x, const MachineInteger &y) {
	return Heap::MachineReal(x.value * y.value);
}

inline BaseExpressionRef operator*(const MachineInteger &x, const MachineReal &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const MachineReal &x, const MachineReal &y) {
	return Heap::MachineReal(x.value * y.value);
}

inline BaseExpressionRef operator*(const MachineInteger &x, const BigReal &y) {
    arb_t r;
    arb_init(r);
    arb_mul_si(r, y.value, long(x.value), y.prec.bits);
    return Heap::BigReal(r, y.prec);
}

inline BaseExpressionRef operator*(const BigReal &x, const MachineInteger &y) {
    return y + x;
}

inline BaseExpressionRef operator*(const BigInteger &x, const MachineReal &y) {
    return Heap::MachineReal(x.value.get_d() * y.value);
}

inline BaseExpressionRef operator*(const MachineReal &x, const BigInteger &y) {
    return y * x;
}

inline BaseExpressionRef operator*(const MachineReal &x, const BigReal &y) {
    return Heap::MachineReal(x.value * y.as_double());
}

inline BaseExpressionRef operator*(const BigReal &x, const MachineReal &y) {
    return y * x;
}

inline BaseExpressionRef operator*(const BigReal &x, const BigReal &y) {
    arb_t r;
    arb_init(r);
    arb_mul(r, x.value, y.value, std::min(x.prec.bits, y.prec.bits));
    return Heap::BigReal(r, x.prec.bits < y.prec.bits ? x.prec : y.prec);
}

inline BaseExpressionRef operator*(const BigInteger &x, const BigReal &y) {
    arf_t temp;
    arf_init(temp);
    arf_set_mpz(temp, x.value.get_mpz_t());

    arb_t r;
    arb_init(r);
    arb_mul_arf(r, y.value, temp, y.prec.bits);

    arf_clear(temp);

    return Heap::BigReal(r, y.prec);
}

inline BaseExpressionRef operator*(const BigReal &x, const BigInteger &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const Rational &x, const BigReal &y) {
	arf_t num;
	arf_init(num);
	arf_set_mpz(num, x.value.get_num().get_mpz_t());

	arf_t den;
	arf_init(den);
	arf_set_mpz(den, x.value.get_den().get_mpz_t());

	arf_t q;
	arf_init(q);
	arf_div(q, num, den, y.prec.bits, ARF_RND_DOWN);

	arb_t r;
	arb_init(r);
	arb_mul_arf(r, y.value, q, y.prec.bits);

	arf_clear(num);
	arf_clear(den);
	arf_clear(q);

	return Heap::BigReal(r, y.prec);
}

inline BaseExpressionRef operator*(const BigReal &x, const Rational &y) {
	return y * x;
}

inline BaseExpressionRef operator*(const Rational &x, const Rational &y) {
	return Heap::Rational(x.value * y.value);
}

namespace Numeric {

	class Z {
	private:
		long machine_value;
		mpz_t big_value;
		bool is_big;

		template<bool c_is_a>
		inline static void add(Z &c, const Z &a, const Z &b) {
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
					mpz_add_ui(c.big_value, c.big_value, (unsigned long) value);
				} else {
					mpz_sub_ui(c.big_value, c.big_value, (unsigned long) -value);
				}
			}
		}

		template<bool c_is_a>
		inline static void mul(Z &c, const Z &a, const Z &b) {
			if (!a.is_big) {
				if (!b.is_big) {
					long r;
					if (!__builtin_smull_overflow(a.machine_value, b.machine_value, &r)) {
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
				mpz_mul(c.big_value, c.big_value, b.big_value);
			} else {
				mpz_mul_si(c.big_value, c.big_value, b.machine_value);
			}
		}

		inline Z() {
		}

	public:
		inline explicit Z(machine_integer_t value) : machine_value(value), is_big(false) {
		}

		inline Z(Z &&x) {
			if (x.is_big) {
				is_big = true;
				std::memcpy(&big_value, &x.big_value, sizeof(mpz_t));
				x.is_big = false;
			} else {
				is_big = false;
				machine_value = x.machine_value;
			}
		}

		inline Z(const Z &x) {
			if (x.is_big) {
				is_big = true;
				mpz_init_set(big_value, x.big_value);
			} else {
				is_big = false;
				machine_value = x.machine_value;
			}
		}

		inline Z(const mpz_class &value) : is_big(true) {
			mpz_init_set(big_value, value.get_mpz_t());
		}

		inline ~Z() {
			if (is_big) {
				mpz_clear(big_value);
			}
		}

		inline Z operator+(const Z &other) const {
			Z x;
			add<false>(x, *this, other);
			return x;
		}

		inline Z &operator+=(const Z &other) {
			add<true>(*this, *this, other);
			return *this;
		}

		inline Z operator*(const Z &other) const {
			Z x;
			mul<false>(x, *this, other);
			return x;
		}

		inline Z &operator*=(const Z &other) {
			mul<true>(*this, *this, other);
			return *this;
		}

		mpz_class to_primitive() const {
			if (!is_big) {
				return machine_integer_to_mpz(machine_value);
			} else {
				return mpz_class(big_value);
			}
		}

		inline BaseExpressionRef to_expression() const {
			if (is_big) {
				return Heap::BigInteger(std::move(mpz_class(big_value)));
			} else {
				return Heap::MachineInteger(machine_integer_t(machine_value));
			}
		}

	};

	class R {
	private:
		arb_t m_value;
        bool m_owned;

	public:
        inline explicit R(const arb_t value) {
            std::memcpy(m_value, value, sizeof(arb_t));
            m_owned = false;
        }

		inline explicit R(machine_integer_t value) {
			arb_init(m_value);
			arb_set_si(m_value, value);
            m_owned = true;
		}

		inline explicit R(machine_real_t value) {
			arb_init(m_value);
			arb_set_d(m_value, value);
            m_owned = true;
		}

		inline R(R &&x) {
            std::memcpy(m_value, x.m_value, sizeof(arb_t));
            m_owned = x.m_owned;
            x.m_owned = false;
		}

		inline R(const R &x) {
            std::memcpy(m_value, x.m_value, sizeof(arb_t));
            m_owned = x.m_owned;
		}

		inline R(const mpz_class &value) {
			arf_t temp;
			arf_init(temp);
			arf_set_mpz(temp, value.get_mpz_t());

			arb_init(m_value);
			arb_set_arf(m_value, temp);
			m_owned = true;

			arf_clear(temp);

		}

		inline R(const mpq_class &value, const Precision &prec) {
			arf_t num;
			arf_init(num);
			arf_set_mpz(num, value.get_num().get_mpz_t());

			arf_t den;
			arf_init(den);
			arf_set_mpz(den, value.get_den().get_mpz_t());

			arf_t q;
			arf_init(q);
			arf_div(q, num, den, prec.bits, ARF_RND_DOWN);

			arb_init(m_value);
			arb_set_arf(m_value, q);
			m_owned = true;

			arf_clear(num);
			arf_clear(den);
			arf_clear(q);
		}

		inline ~R() {
            if (m_owned) {
                arb_clear(m_value);
            }
		}

        inline bool operator<(const R &r) const {
            return arb_lt(m_value, r.m_value);
        }

        inline bool operator<=(const R &r) const {
            return arb_le(m_value, r.m_value);
        }

        inline bool operator>(const R &r) const {
            return arb_gt(m_value, r.m_value);
        }

        inline bool operator>=(const R &r) const {
            return arb_ge(m_value, r.m_value);
        }
	};

} // end of namespace Numeric

template<>
struct Comparison<MachineInteger, BigReal> {
    template<typename F>
    inline static bool compare(const MachineInteger &u, const BigReal &v, const F &f) {
        return f(Numeric::R(u.value), Numeric::R(v.value));
    }
};

template<>
struct Comparison<BigReal, MachineInteger> {
    template<typename F>
    inline static bool compare(const BigReal &u, const MachineInteger &v, const F &f) {
        return f(Numeric::R(u.value), Numeric::R(v.value));
    }
};

template<>
struct Comparison<MachineReal, BigReal> {
    template<typename F>
    inline static bool compare(const MachineReal &u, const BigReal &v, const F &f) {
        return f(Numeric::R(u.value), Numeric::R(v.value));
    }
};

template<>
struct Comparison<BigReal, MachineReal> {
    template<typename F>
    inline static bool compare(const BigReal &u, const MachineReal &v, const F &f) {
        return f(Numeric::R(u.value), Numeric::R(v.value));
    }
};

template<>
struct Comparison<BigInteger, BigReal> {
    template<typename F>
    inline static bool compare(const BigInteger &u, const BigReal &v, const F &f) {
        return f(Numeric::R(u.value), Numeric::R(v.value));
    }
};

template<>
struct Comparison<BigReal, BigInteger> {
    template<typename F>
    inline static bool compare(const BigReal &u, const BigInteger &v, const F &f) {
        return f(Numeric::R(u.value), Numeric::R(v.value));
    }
};

template<>
struct Comparison<Rational, BigReal> {
	template<typename F>
	inline static bool compare(const Rational &u, const BigReal &v, const F &f) {
		return f(Numeric::R(u.value, v.prec), Numeric::R(v.value));
	}
};

template<>
struct Comparison<BigReal, Rational> {
	template<typename F>
	inline static bool compare(const BigReal &u, const Rational &v, const F &f) {
		return f(Numeric::R(u.value), Numeric::R(v.value, u.prec));
	}
};

#endif //CMATHICS_NUMERIC_H
