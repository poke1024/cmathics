#pragma once

#include "collection.h"
#include "generator.h"

class RefsExtent : public Shared<RefsExtent, SharedPool> {
private:
    const std::vector<BaseExpressionRef> m_data;

public:
    inline explicit RefsExtent(const std::vector<BaseExpressionRef> &data) : m_data(data) {
    }

    inline explicit RefsExtent(std::vector<BaseExpressionRef> &&data) : m_data(data) {
    }

    inline explicit RefsExtent(const std::initializer_list<BaseExpressionRef> &data) : m_data(data) {
    }

    inline const BaseExpressionRef *address() const {
        return &m_data[0];
    }

    inline size_t size() const {
        return m_data.size();
    }
};

inline RefsExtentRef Pool::RefsExtent(const std::vector<BaseExpressionRef> &data) {
    return RefsExtentRef(_s_instance->_refs_extents.construct(data));
}

inline RefsExtentRef Pool::RefsExtent(std::vector<BaseExpressionRef> &&data) {
    return RefsExtentRef(_s_instance->_refs_extents.construct(data));
}

inline RefsExtentRef Pool::RefsExtent(const std::initializer_list<BaseExpressionRef> &data) {
    return RefsExtentRef(_s_instance->_refs_extents.construct(data));
}

inline void Pool::release(class RefsExtent *extent) {
    _s_instance->_refs_extents.destroy(extent);
}

class BigSlice : public BaseRefsSlice<SliceCode::BigSliceCode> {
private:
    RefsExtentRef m_extent;

    inline BigSlice(const RefsExtentRef &extent, TypeMask type_mask) :
        BaseRefsSlice(extent->address(), extent->size(), type_mask),
        m_extent(extent) {
    }

public:
    inline const BaseExpressionRef *begin() const {
        return m_address;
    }

    inline const BaseExpressionRef *end() const {
        return m_address + m_size;
    }

    inline const BaseExpressionRef &operator[](size_t i) const {
        return m_address[i];
    }

    inline size_t size() const {
        const size_t n = m_size;
        if (n <= MaxTinySliceSize) {
            __builtin_unreachable();
        }
        return n;
    }

public:
    template<typename V>
    using PrimitiveCollection = PointerCollection<BaseExpressionRef, BaseExpressionToPrimitive<V>>;

    using LeafCollection = PointerCollection<BaseExpressionRef>;

    inline LeafCollection leaves() const {
        return LeafCollection(begin(), size());
    }

    template<typename V>
    inline PrimitiveCollection<V> primitives() const {
        return PrimitiveCollection<V>(begin(), size(), BaseExpressionToPrimitive<V>());
    }

public:
    inline BigSlice(const BigSlice &slice) :
        BaseRefsSlice(slice.begin(), slice.size(), slice.m_type_mask),
        m_extent(slice.m_extent) {
    }

    inline BigSlice() : BaseRefsSlice(nullptr, 0, 0) {
    }

    inline BigSlice(LeafVector &&leaves) :
        BigSlice(Pool::RefsExtent(leaves.unsafe_grab_internal_vector()), leaves.type_mask()) {

        assert(leaves.size() > MaxTinySliceSize);
    }

    template<typename F>
    inline BigSlice(const FSGenerator<F> &generator) : BigSlice(generator.vector()) {
    }

    template<typename F>
    inline BigSlice(const FPGenerator<F> &generator) : BigSlice(generator.vector()) {
    }

    inline BigSlice(const std::initializer_list<BaseExpressionRef> &data, TypeMask type_mask) :
        BigSlice(Pool::RefsExtent(data), type_mask) {

        assert(data.size() > MaxTinySliceSize);
    }

    inline BigSlice(
        const RefsExtentRef &extent,
        const BaseExpressionRef * const begin,
        const BaseExpressionRef * const end,
        TypeMask type_mask) :
        BaseRefsSlice(begin, end - begin, type_mask),
        m_extent(extent) {

        assert(end - begin > MaxTinySliceSize);
    }

    template<typename F>
    static inline BigSlice create(F &f, size_t n) {
        return BigSlice(sequential(f, n));
    }

    template<typename F>
    static inline BigSlice parallel_create(const F &f, size_t n) {
        return BigSlice(parallel(f, n));
    }

    template<typename F>
    inline BigSlice map(const F &f) const {
        const size_t n = size();
        const auto &slice = *this;
        return BigSlice(sequential([n, &f, &slice] (auto &store) {
            for (size_t i = 0; i < n; i++) {
                store(f(slice[i]));
            }
        }, n));
    }

    template<typename F>
    inline BigSlice parallel_map(const F &f) const {
        const auto &slice = *this;
        return BigSlice(parallel([&f, &slice] (size_t i) {
            return f(slice[i]);
        }, size()));
    }

    inline BigSlice slice(size_t begin, size_t end) const {
        return BigSlice(m_extent, m_address + begin, m_address + end, sliced_type_mask(end - begin));
    }

    template<int M>
    inline BigSlice drop() const {
        return slice(M, size());
    }

    inline bool is_packed() const {
        return false;
    }

    inline BigSlice unpack() const {
        return *this;
    }

    inline const BaseExpressionRef *refs() const {
        return begin();
    }
};
