#pragma once

typedef uint32_t attributes_bitmask_t;

enum class Attributes : attributes_bitmask_t {
    None = 0,
    // pattern matching attributes
    Orderless = 1 << 0,
    Flat = 1 << 1,
    OneIdentity = 1 << 2,
    Listable = 1 << 3,
    // calculus attributes
    Constant = 1 << 4,
    NumericFunction = 1 << 5,
    // rw attributes
    Protected = 1 << 6,
    Locked = 1 << 7,
    ReadProtected = 1 << 8,
    // evaluation hold attributes
    HoldFirst = 1 << 9,
    HoldRest = 1 << 10,
    HoldAll = HoldFirst + HoldRest,
    HoldAllComplete = 1 << 11,
    // evaluation nhold attributes
    NHoldFirst = 1 << 12,
    NHoldRest = 1 << 13,
    NHoldAll = NHoldFirst + NHoldRest,
    // misc attributes
    SequenceHold = 1 << 14,
    Temporary = 1 << 15,
    Stub = 1 << 16
};

inline bool operator&(Attributes x, Attributes y)  {
	if (attributes_bitmask_t(y) == 0) {
		return false;
	} else {
		return (attributes_bitmask_t(x) & attributes_bitmask_t(y)) == attributes_bitmask_t(y);
	}
}

inline size_t count(Attributes x, Attributes y) {
    std::bitset<sizeof(attributes_bitmask_t) * 8> bits(
        attributes_bitmask_t(x) & attributes_bitmask_t(y));
    return bits.count();
}

inline constexpr Attributes operator+(Attributes x, Attributes y)  {
    return Attributes(attributes_bitmask_t(x) | attributes_bitmask_t(y));
}

inline constexpr Attributes operator-(Attributes x, Attributes y)  {
    return Attributes(attributes_bitmask_t(x) & ~attributes_bitmask_t(y));
}
