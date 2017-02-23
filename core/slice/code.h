#pragma once

constexpr int MaxTinySliceSize = 4;
static_assert(MaxTinySliceSize >= 4, "MaxTinySliceSize must be >= 4");

constexpr int MinPackedSliceSize = 16;
static_assert(MinPackedSliceSize > MaxTinySliceSize, "MinPackedSliceSize too small");

enum SliceCode : uint8_t {
    TinySlice0Code = 0,
    TinySlice1Code = 1,
    TinySlice2Code = 2,
    TinySlice3Code = 3,
    TinySlice4Code = 4,
    TinySliceNCode = TinySlice0Code + MaxTinySliceSize,

    BigSliceCode = TinySliceNCode + 1,

    PackedSlice0Code = BigSliceCode + 1,
    PackedSliceMachineIntegerCode = PackedSlice0Code,
    PackedSliceMachineRealCode,
    PackedSliceNCode = PackedSliceMachineRealCode,

    NumberOfSliceCodes = PackedSliceNCode + 1,
    Unknown = 255
};

template<SliceCode slice_code>
struct PackedSliceType {
};

template<>
struct PackedSliceType<PackedSliceMachineIntegerCode> {
    typedef machine_integer_t type;
};

template<>
struct PackedSliceType<PackedSliceMachineRealCode> {
    typedef machine_real_t type;
};

constexpr inline bool is_packed_slice(SliceCode id) {
    return id >= PackedSlice0Code && id <= PackedSliceNCode;
}

constexpr inline bool is_tiny_slice(SliceCode code) {
    static_assert(TinySlice0Code == 0, "TinySlice0Code must be 0");
    return size_t(code) <= TinySliceNCode;
}

constexpr inline bool slice_needs_no_materialize(SliceCode id) {
    return is_tiny_slice(id) || id == SliceCode::BigSliceCode;
}

constexpr inline SliceCode tiny_slice_code(size_t n) {
    const SliceCode code = SliceCode(SliceCode::TinySlice0Code + n);
    assert(code <= TinySliceNCode);
    return code;
}

constexpr inline size_t tiny_slice_size(SliceCode code) {
    return size_t(code) - size_t(TinySlice0Code);
}
