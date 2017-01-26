#ifndef HASH_H
#define HASH_H

#include <gmpxx.h>

typedef size_t hash_t;

inline hash_t djb2(const char* str) {
	hash_t result = 5381;

	int c;
    while ((c = *str++)) {
        result = ((result << 5) + result) + c;
    }

    return result;
}

inline hash_t hash_combine(hash_t seed, const hash_t x) {
    // see boost::hash_combine
    seed ^= x + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
}

inline hash_t hash_pair(const hash_t x, const hash_t y) {
    // combines 2 hash values
	return hash_combine(hash_combine(0, x), y);
}

inline hash_t hash_mpz(const mpz_class &value) {
    std::hash<mp_limb_t> limb_hash;
    const mpz_srcptr x = value.get_mpz_t();
    const size_t n = std::abs(x->_mp_size);
    hash_t h = 0;
    for (size_t i = 0; i < n; i++) {
        h = hash_combine(h, limb_hash(x->_mp_d[i]));
    }
    return h;
}

constexpr hash_t symbol_hash = 0x652d2463adb; // djb2("Symbol")
constexpr hash_t string_hash = 0x652d1ee9bdc; // djb2("String")

constexpr hash_t machine_integer_hash = 0x1874fa90f5a6c248; // djb2("MachineInteger")
constexpr hash_t machine_real_hash = 0xc000a15031b8359e; // djb2("MachineReal")
constexpr hash_t machine_complex_hash = 0x1874fa8f29a5f052; // djb2("MachineComplex")

#endif
