#ifndef HASH_H
#define HASH_H

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

constexpr hash_t symbol_hash = 0x652d2463adb; // djb2("Symbol")
constexpr hash_t string_hash = 0x652d1ee9bdc; // djb2("String")

constexpr hash_t machine_integer_hash = 0x1874fa90f5a6c248; // djb2("MachineInteger")
constexpr hash_t machine_real_hash = 0xc000a15031b8359e; // djb2("MachineReal")

#endif
