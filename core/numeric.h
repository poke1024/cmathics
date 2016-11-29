#ifndef CMATHICS_NUMERIC_H
#define CMATHICS_NUMERIC_H

#include <gmpxx.h>

#include "types.h"
#include "integer.h"

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
		static_assert(sizeof(machine_integer_t) == sizeof(decltype(machine_value)),
              "machine integer type must equivalent to long");

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

		/*Z(const mpq_class &value) {
			throw std::runtime_error("cannot create Numeric::Z from mpq_class");
		}

		Z(const std::string &value) {
			throw std::runtime_error("cannot create Numeric::Z from std::string");
		}*/

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
				return from_primitive(std::move(mpz_class(big_value)));
			} else {
				return from_primitive(machine_integer_t(machine_value));
			}
		}

	};

} // end of namespace Numeric

inline BaseExpressionRef from_primitive(const Numeric::Z &value) {
	return value.to_expression();
}


#endif //CMATHICS_NUMERIC_H
