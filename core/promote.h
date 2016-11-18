#ifndef CMATHICS_PROMOTE_H
#define CMATHICS_PROMOTE_H

// rules for type promotion

template<typename V, typename U>
inline V promote(const U &u) {
    throw std::runtime_error("unsupported promotion");
}

#define PROMOTE(U, V) template<> \
    inline V promote<V, U>(const U &x)

#define NO_PROMOTE(U, V) template<> \
    inline V promote<V, U>(const U &x) { \
    throw std::runtime_error("unsupported promotion"); }

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
	mpz_t big_value;
	mpz_init_set_si(big_value, x);
	const mpz_class y(big_value);
	mpz_clear(big_value);
	return y;
}

NO_PROMOTE(mpq_class, mpint)
NO_PROMOTE(std::string, mpint)

// machine_real_t

PROMOTE(machine_real_t, machine_real_t) {
    return x;
}

PROMOTE(machine_real_t, mpfr::mpreal) {
    return x;
}

NO_PROMOTE(std::string, machine_real_t)
NO_PROMOTE(mpq_class, machine_real_t)
NO_PROMOTE(mpz_class, machine_real_t)

#endif //CMATHICS_PROMOTE_H
