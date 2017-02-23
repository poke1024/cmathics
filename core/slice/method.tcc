#include "array.tcc"
#include "vcall.tcc"
#include "big.h"
#include "packed.h"

template<typename R, typename F>
class SliceMethod<CompileToSliceType, R, F> {
private:
    std::function<R(const F&, const Expression *)> m_implementations[NumberOfSliceCodes];

    template<SliceCode slice_code>
    void init_tiny_slice() {
        m_implementations[slice_code] = [] (const F &f, const Expression *expr) {
            const auto cast = [] (auto ptr) {
                return static_cast<const TinySlice<slice_code - TinySlice0Code>*>(ptr);
            };

            return f.lambda(*cast(expr->_slice_ptr));
        };

        STATIC_IF (slice_code < TinySliceNCode) {
            init_tiny_slice<SliceCode(slice_code + 1)>();
        } STATIC_ENDIF
    }

    template<SliceCode slice_code>
    void init_packed_slice() {
        m_implementations[slice_code] = [] (const F &f, const Expression *expr) {
            const auto cast = [] (auto ptr) {
                return static_cast<const PackedSlice<typename PackedSliceType<slice_code>::type>*>(ptr);
            };

            return f.lambda(*cast(expr->_slice_ptr));
        };

        STATIC_IF (slice_code < PackedSliceNCode) {
            init_packed_slice<SliceCode(slice_code + 1)>();
        } STATIC_ENDIF
    }

public:
    SliceMethod() {
        init_tiny_slice<TinySlice0Code>();

        m_implementations[BigSliceCode] = [] (const F &f, const Expression *expr) {
            return f.lambda(*static_cast<const BigSlice*>(expr->_slice_ptr));
        };

        init_packed_slice<PackedSlice0Code>();
    }

    inline R operator()(const F &f, const Expression *expr) const {
        return m_implementations[expr->slice_code()](f, expr);
    }
};

template<typename R, typename F>
class SliceMethod<CompileToPackedSliceType, R, F> {
private:
    std::function<R(const F&, const Expression *)> m_implementations[1 + PackedSliceNCode - PackedSlice0Code];

    template<SliceCode slice_code>
    void init_packed_slice() {
        m_implementations[slice_code - PackedSlice0Code] = [] (const F &f, const Expression *expr) {
            const auto cast = [] (auto ptr) {
                return static_cast<const PackedSlice<typename PackedSliceType<slice_code>::type>*>(ptr);
            };

            return f(*cast(expr->_slice_ptr));
        };

        STATIC_IF (slice_code < PackedSliceNCode) {
            init_packed_slice<SliceCode(slice_code + 1)>();
        } STATIC_ENDIF
    }

public:
    SliceMethod() {
        init_packed_slice<PackedSlice0Code>();
    }

    inline R operator()(const F &f, const Expression *expr) const {
        const BaseExpressionRef * const leaves = expr->_slice_ptr->m_address;
        if (leaves) {
            const ArraySlice slice(leaves, expr);
            return f(slice);
        } else {
            const SliceCode slice_code = expr->slice_code();
            assert(is_packed_slice(slice_code));
            return m_implementations[slice_code - PackedSlice0Code](f, expr);
        }
    }
};

template<typename AccessLeaf>
class ByIndexCollection {
private:
    const AccessLeaf m_access_leaf;
    const size_t m_size;

public:
    class Iterator {
    private:
        const AccessLeaf m_access_leaf;
        size_t m_index;

    public:
        explicit Iterator(const AccessLeaf &access_leaf, size_t index) :
            m_access_leaf(access_leaf), m_index(index) {
        }

        inline auto operator*() const {
            return m_access_leaf(m_index);
        }

        inline bool operator==(const Iterator &other) const {
            return m_index == other.m_index;
        }

        inline bool operator!=(const Iterator &other) const {
            return m_index != other.m_index;
        }

        inline Iterator &operator++() {
            m_index += 1;
            return *this;
        }

        inline Iterator operator++(int) const {
            return Iterator(m_access_leaf, m_index + 1);
        }
    };

    inline ByIndexCollection(const AccessLeaf &access_leaf, size_t size) :
        m_access_leaf(access_leaf), m_size(size) {
    }

    inline Iterator begin() const {
        return Iterator(m_access_leaf, 0);
    }

    inline Iterator end() const {
        return Iterator(m_access_leaf, m_size);
    }

    inline auto operator[](size_t i) const {
        return m_access_leaf(i);
    }
};

template<typename R, typename F>
class SliceMethod<DoNotCompileToSliceType, R, F> {
public:
    SliceMethod() {
    }

    inline R operator()(const F &f, const Expression *expr) const {
        const BaseExpressionRef * const leaves = expr->_slice_ptr->m_address;
        if (leaves) {
            const ArraySlice slice(leaves, expr);
            return f.lambda(slice);
        } else {
            const VCallSlice slice(expr);
            return f.lambda(slice);
        }
    }
};
