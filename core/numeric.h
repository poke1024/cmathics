#ifndef CMATHICS_NUMERIC_H
#define CMATHICS_NUMERIC_H

#include <gmpxx.h>
#include <arb.h>

#include "types.h"
#include "integer.h"
#include "real.h"
#include "rational.h"
#include "heap.h"

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

        inline explicit Z(const char *s) {
            mpz_init(big_value);
            mpz_set_str(big_value, s, 10);
            if (mpz_fits_slong_p(big_value)) {
                is_big = false;
                machine_value = mpz_get_si(big_value);
                mpz_clear(big_value);
            } else {
                is_big = true;
            }
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

		bool operator<=(const Z &other) {
			if (!is_big) {
				if (!other.is_big) {
					return machine_value < other.machine_value;
				} else {
					return mpz_cmp_si(other.big_value, machine_value) >= 0;
				}
			} else {
				if (!other.is_big) {
					return mpz_cmp_si(big_value, other.machine_value) <= 0;
				} else {
					return mpz_cmp(big_value, other.big_value) <= 0;
				}
			}
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
				return Pool::BigInteger(std::move(mpz_class(big_value)));
			} else {
				return Pool::MachineInteger(machine_integer_t(machine_value));
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

        inline bool operator==(const R &r) const {
            return arb_eq(m_value, r.m_value);
        }

        inline bool operator!=(const R &r) const {
            return arb_ne(m_value, r.m_value);
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

#endif //CMATHICS_NUMERIC_H
