#ifndef CMATHICS_PROMOTE_H
#define CMATHICS_PROMOTE_H

#include "integer.h"
#include <static_if/static_if.hpp>

// rules for type promotion

template<bool u_is_v, typename V, typename U>
struct special_promote {
};

template<typename V, typename U>
struct special_promote<false, V, U> {
    V operator()(const U &u) const {
        std::ostringstream s;
        s << "unsupported promotion from " << typeid(u).name() << " to " << typeid(V).name();
        throw std::runtime_error(s.str());
    }
};

template<typename V, typename U>
struct special_promote<true, V, U> {
    inline U operator()(const U &u) const {
        return u;
    }
};

template<typename V, typename U>
inline V promote(const U &u) {
    return special_promote<std::is_same<U, V>::value, V, U>()(u);
}

#define PROMOTE(U, V) template<> \
    inline V promote<V, U>(const U &x)

// mpint

PROMOTE(mpz_class, mpint) {
    return mpint(x);
}

PROMOTE(machine_integer_t, mpint) {
    return mpint(x);
}

PROMOTE(machine_integer_t, mpfr::mpreal) {
	return x;
}

PROMOTE(machine_integer_t, mpz_class) {
    return machine_integer_to_mpz(x);
}

PROMOTE(machine_integer_t, mpq_class) {
    return mpq_class(machine_integer_to_mpz(x), machine_integer_to_mpz(1));
}

// machine_real_t

PROMOTE(machine_real_t, machine_real_t) {
    return x;
}

PROMOTE(machine_real_t, mpfr::mpreal) {
    return x;
}

// mpz_class

PROMOTE(mpz_class, machine_real_t) {
    return x.get_d();
}

// mpq_class

PROMOTE(mpq_class, machine_real_t) {
    return x.get_d();
}


#endif //CMATHICS_PROMOTE_H
