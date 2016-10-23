#ifndef HASH_H
#define HASH_H

typedef uint64_t hash_t;

inline hash_t djb2(const char* str) {
    uint64_t result;
    int c;

    result = 5381;
    while ((c = *str++)) {
        result = ((result << 5) + result) + c;
    }
    return result;
}


inline hash_t hash_combine(uint64_t seed, const uint64_t x) {
    // see boost::hash_combine
    seed ^= x + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
}


inline hash_t hash_pair(const uint64_t x, const uint64_t y) {
    // combines 2 hash values
    uint64_t seed = 0;
    seed = hash_combine(seed, x);
    seed = hash_combine(seed, y);
    return seed;
}


extern const hash_t symbol_hash;
extern const hash_t machine_integer_hash;
extern const hash_t machine_real_hash;
extern const hash_t string_hash;
extern const hash_t rational_hash;


#endif
