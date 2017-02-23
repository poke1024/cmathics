#pragma once

#include <cmath>
#include <mpfr.h>

class Precision {
private:
    static inline mp_prec_t to_bits_prec(double prec) {
        constexpr double LOG_2_10 = 3.321928094887362; // log2(10.0);
        return static_cast<mp_prec_t>(std::ceil(LOG_2_10 * prec));
    }

    static inline mp_prec_t from_bits_prec(mp_prec_t bits_prec) {
        constexpr double LOG_2_10 = 3.321928094887362; // log2(10.0);
        return bits_prec / LOG_2_10;
    }

public:
    static const Precision none;
    static const Precision machine_precision;

    inline explicit Precision(double decimals_) : decimals(decimals_), bits(to_bits_prec(decimals_)) {
    }

    inline explicit Precision(mp_prec_t bits_) : bits(bits_), decimals(from_bits_prec(bits_)) {
    }

    inline Precision(const Precision &p) : decimals(p.decimals), bits(p.bits) {
    }

    inline bool is_machine_precision() const {
        return bits == std::numeric_limits<machine_real_t>::digits;
    }

    inline bool is_none() const {
        return bits == 0;
    }

    const double decimals;
    const mp_prec_t bits;
};

inline bool operator<(const Precision &u, const Precision &v) {
    return u.bits < v.bits;
}

inline bool operator>(const Precision &u, const Precision &v) {
    return u.bits > v.bits;
}

Precision precision(const BaseExpressionRef&);
