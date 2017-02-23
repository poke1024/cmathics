#pragma once

#include <gmpxx.h>
#include <arb.h>

inline mpz_class machine_integer_to_mpz(machine_integer_t machine_value) {
    mpz_class value;
    value = long(machine_value);
    return value;
}

#include "precision.h"

namespace Numeric {

	class Z {
    private:
        static_assert(sizeof(long) == sizeof(machine_integer_t),
          "types long and machine_integer_t must not differ for Numeric::Z to work");

		long machine_value;
		mpz_t big_value;
		bool is_big;

		template<bool c_is_a>
		inline static void add(Z &c, const Z &a, const Z &b) {
			if (!a.is_big) {
				if (!b.is_big) {
					long r;
					if (!__builtin_saddl_overflow(a.machine_value, b.machine_value, &r)) {
						assert(!c.is_big);
						c.machine_value = r;
						return;
					}
				}
                assert(!c.is_big);
				mpz_init_set_si(c.big_value, a.machine_value);
				c.is_big = true;
			} else if (!c_is_a) { // a.is_big && c != a
                assert(!c.is_big);
                mpz_init_set(c.big_value, a.big_value);
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
                        assert(!c.is_big);
						c.machine_value = r;
						return;
					}
				}
                assert(!c.is_big);
				mpz_init_set_si(c.big_value, a.machine_value);
				c.is_big = true;
			} else if (!c_is_a) { // a.is_big && c != a
                assert(!c.is_big);
                mpz_init_set(c.big_value, a.big_value);
                c.is_big = true;
            }
			if (b.is_big) {
				mpz_mul(c.big_value, c.big_value, b.big_value);
			} else {
				mpz_mul_si(c.big_value, c.big_value, b.machine_value);
			}
		}

        template<bool c_is_a>
        inline static void quotient(Z &c, const Z &a, const Z &b) {
            if (!a.is_big) {
                if (!b.is_big && a.machine_value > 0 && b.machine_value > 0) {
                    // if a or b are negative, there are several nasty border:
                    // cases: C++ integer div truncates instead of floors (for
                    // C89, it's even implementation dependent), and there are
                    // overflowing border cases (e.g. x / -1). so we just let
                    // mpz deal with all this.

                    assert(!c.is_big);
                    c.machine_value = a.machine_value / b.machine_value;
                    return;
                } else {
                    assert(!c.is_big);
                    mpz_init_set_si(c.big_value, a.machine_value);
                    c.is_big = true;
                }
            } else if (!c_is_a) { // a.is_big && c != a
                assert(!c.is_big);
                mpz_init_set(c.big_value, a.big_value);
                c.is_big = true;
            }

            if (!b.is_big && b.machine_value > 0) {
                mpz_fdiv_q_ui(c.big_value, c.big_value, b.machine_value);
            } else if (b.is_big) {
                mpz_fdiv_q(c.big_value, c.big_value, b.big_value);
            } else {
                mpz_t t;
                mpz_init_set_si(t, b.machine_value);
                mpz_fdiv_q(c.big_value, c.big_value, t);
                mpz_clear(t);
            }

            assert(c.is_big);
            if (mpz_fits_slong_p(c.big_value)) {
                c.machine_value = mpz_get_si(c.big_value);
                mpz_clear(c.big_value);
                c.is_big = false;
            }
        }

		inline Z() : is_big(false) {
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

        inline bool is_zero() const {
            return is_big ?
               mpz_cmp_ui(big_value, 0) == 0 :
               machine_value == 0;
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

        inline Z operator/(const Z &other) const {
            Z x;
            quotient<false>(x, *this, other);
            return x;
        }

        inline Z &operator/=(const Z &other) {
            quotient<true>(*this, *this, other);
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

		inline BaseExpressionRef to_expression() const;
	};

	class R {
	private:
		mpfr_t m_value;
        bool m_owned;

	public:
        inline explicit R(const mpfr_t value) {
            std::memcpy(m_value, value, sizeof(arb_t));
            m_owned = false;
        }

		inline explicit R(machine_integer_t value) {
			mpfr_init(m_value);
			mpfr_set_si(m_value, value, MPFR_RNDN);
            m_owned = true;
		}

		inline explicit R(machine_real_t value) {
			mpfr_init(m_value);
			mpfr_set_d(m_value, value, MPFR_RNDN);
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
			mpfr_init(m_value);
			mpfr_set_z(m_value, value.get_mpz_t(), MPFR_RNDN);
			m_owned = true;
		}

		inline R(const mpq_class &value, const Precision &prec) {
			/*arf_t num;
			arf_init(num);
			arf_set_mpz(num, value.get_num().get_mpz_t());

			arf_t den;
			arf_init(den);
			arf_set_mpz(den, value.get_den().get_mpz_t());

			arf_t q;
			arf_init(q);
			arf_div(q, num, den, prec.bits, ARF_RND_DOWN);*/

			mpfr_init2(m_value, prec.bits);
			mpfr_set_q(m_value, value.get_mpq_t(), MPFR_RNDN);
			m_owned = true;

			/*arf_clear(num);
			arf_clear(den);
			arf_clear(q);*/
		}

		inline ~R() {
            if (m_owned) {
                mpfr_clear(m_value);
            }
		}

        inline bool operator==(const R &r) const {
            return mpfr_cmp(m_value, r.m_value) == 0;
        }

        inline bool operator!=(const R &r) const {
            return mpfr_cmp(m_value, r.m_value) != 0;
        }

        inline bool operator<(const R &r) const {
            return mpfr_cmp(m_value, r.m_value) < 0;
        }

        inline bool operator<=(const R &r) const {
            return mpfr_cmp(m_value, r.m_value) <= 0;
        }

        inline bool operator>(const R &r) const {
            return mpfr_cmp(m_value, r.m_value) > 0;
        }

        inline bool operator>=(const R &r) const {
            return mpfr_cmp(m_value, r.m_value) >= 0;
        }
	};

} // end of namespace Numeric
