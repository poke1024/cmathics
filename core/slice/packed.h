#pragma once

#include "collection.h"

template<typename U>
class PackExtent : public Shared<PackExtent<U>, SharedHeap> {
private:
    const std::vector<U> m_data;

public:
    typedef ConstSharedPtr<PackExtent<U>> Ref;

    inline explicit PackExtent(const std::vector<U> &data) : m_data(data) {
    }

    inline explicit PackExtent(std::vector<U> &&data) : m_data(data) {
    }

    inline const std::vector<U> &data() const {
        return m_data;
    }

    inline const U *address() {
        return &m_data[0];
    }

    inline size_t size() const {
        return m_data.size();
    }
};

template<typename U>
struct PackedSliceInfo {
};

template<>
struct PackedSliceInfo<machine_integer_t> {
    static constexpr SliceCode code = PackedSliceMachineIntegerCode;
};

template<>
struct PackedSliceInfo<machine_real_t> {
    static constexpr SliceCode code = PackedSliceMachineRealCode;
};

template<typename U>
class PackedSlice : public TypedSlice<PackedSliceInfo<U>::code> {
private:
    typename PackExtent<U>::Ref _extent;
    const U * const _begin;

public:
    template<typename V>
    using PrimitiveCollection = PointerCollection<U, PromotePrimitive<V>>;

    using LeafCollection = PointerCollection<U, PrimitiveToBaseExpression<U>>;

    using BaseSlice = TypedSlice<PackedSliceInfo<U>::code>;

public:
    inline size_t size() const {
        const size_t n = BaseSlice::m_size;
        if (n < MinPackedSliceSize) {
            __builtin_unreachable();
        }
        return n;
    }

    template<typename F>
    inline PackedSlice(const F &f, size_t n) {
        throw std::runtime_error("not implemented");
    }

    inline PackedSlice(const std::vector<U> &data) :
        _extent(new PackExtent<U>(data)),
        _begin(_extent->address()),
        BaseSlice(nullptr, data.size()) {
        assert(data.size() >= MinPackedSliceSize);
    }

    inline PackedSlice(std::vector<U> &&data) :
        _extent(new PackExtent<U>(data)),
        _begin(_extent->address()),
        BaseSlice(nullptr, data.size()) {
        assert(data.size() >= MinPackedSliceSize);
    }

    inline PackedSlice(const typename PackExtent<U>::Ref &extent, const U *begin, size_t size) :
        _extent(extent),
        _begin(begin),
        BaseSlice(nullptr, size) {
        assert(size >= MinPackedSliceSize);
    }

    template<typename F>
    static inline PackedSlice create(F &f, size_t n) {
        throw std::runtime_error("not implemented");
    }

    template<typename F>
    static inline PackedSlice parallel_create(const F &f, size_t n) {
        throw std::runtime_error("not implemented");
    }

    template<typename F>
    inline BigSlice map(const F &f) const;

    template<typename F>
    inline BigSlice parallel_map(const F &f) const;

    inline PackedSlice<U> slice(size_t begin, size_t end) const {
        assert(end - begin >= MinPackedSliceSize);
        return PackedSlice<U>(_extent, _begin + begin, end - begin);
    }

    template<int M>
    inline PackedSlice<U> drop() const {
        return slice(M, size());
    }

    inline constexpr TypeMask type_mask() const {
        // constexpr is important here, as it allows apply() to optimize this for most cases that
        // need specific type masks (e.g. optimize evaluate of leaves on PackSlices to a noop).
        return make_type_mask(TypeFromPrimitive<U>::type);
    }

    inline constexpr TypeMask exact_type_mask() const {
        return type_mask();
    }

    inline void init_type_mask(TypeMask type_mask) const {
        // noop
    }

    template<typename V>
    PrimitiveCollection<V> primitives() const {
        return PrimitiveCollection<V>(_begin, size());
    }

    LeafCollection leaves() const {
        return LeafCollection(_begin, size());
    }

    inline BaseExpressionRef operator[](size_t i) const;

    inline bool is_packed() const {
        return true;
    }

    BigSlice unpack() const;

    inline const BaseExpressionRef *refs() const {
        throw std::runtime_error("cannot get refs on PackSlice");
    }
};

template<typename U>
template<typename F>
inline BigSlice PackedSlice<U>::map(const F &f) const {
    const size_t n = size();
    const auto &slice = *this;
    return BigSlice(sequential([n, &f, &slice] (auto &store) {
        for (size_t i = 0; i < n; i++) {
            store(f(slice[i]));
        }
    }, n));
}

template<typename U>
template<typename F>
inline BigSlice PackedSlice<U>::parallel_map(const F &f) const {
    const auto &slice = *this;
    return BigSlice(parallel([&f, &slice] (size_t i) {
        return f(slice[i]);
    }, size()));
}

template<typename U>
inline BigSlice PackedSlice<U>::unpack() const {
    LeafVector leaves;
    for (auto leaf : this->leaves()) {
        leaves.push_back_copy(leaf);
    }
    return BigSlice(std::move(leaves));
}

template<typename U>
inline BaseExpressionRef PackedSlice<U>::operator[](size_t i) const {
    return from_primitive(_begin[i]);
}
