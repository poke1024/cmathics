#pragma once

class Slice {
public:
    const size_t m_size;
    const BaseExpressionRef * const m_address;

    inline Slice(const BaseExpressionRef *address, size_t size) :
        m_address(address), m_size(size) {
    }
};

template<SliceCode Code>
class TypedSlice : public Slice {
public:
    using Slice::Slice;

    static constexpr SliceCode code() {
        return Code;
    }
};

template<SliceCode Code>
class BaseRefsSlice : public TypedSlice<Code> {
public:
    mutable std::atomic<TypeMask> m_type_mask;

public:
    inline TypeMask sliced_type_mask(size_t new_size) const {
        if (new_size == 0) {
            return 0;
        } else {
            const TypeMask mask = m_type_mask;

            if (is_exact_type_mask(mask)) {
                if (is_homogenous(mask)) {
                    return mask;
                } else {
                    return mask | TypeMaskIsInexact;
                }
            } else {
                return mask;
            }
        }
    }

    inline TypeMask type_mask() const {
        return m_type_mask;
    }

    inline TypeMask exact_type_mask() const {
        const TypeMask mask = m_type_mask;

        if (is_exact_type_mask(mask)) {
            return mask;
        } else {
            const BaseExpressionRef *p = Slice::m_address;
            const BaseExpressionRef *p_end = p + Slice::m_size;

            TypeMask new_mask = 0;
            while (p != p_end) {
                new_mask |= (*p++)->type_mask();
            }

            m_type_mask = mask;
            return new_mask;
        }
    }

    inline void init_type_mask(TypeMask type_mask) const {
        m_type_mask = type_mask;
    }

public:
    inline BaseRefsSlice(const BaseExpressionRef *address, size_t size, TypeMask type_mask) :
        TypedSlice<Code>(address, size), m_type_mask(type_mask) {
    }
};

template<typename T>
inline TypeMask exact_type_mask(const T &container) {
    TypeMask mask = 0;
    for (auto leaf : container) {
        mask |= leaf->type_mask();
    }
    return mask;
}
