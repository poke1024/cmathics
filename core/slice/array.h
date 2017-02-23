#pragma once

class ArraySlice {
private:
	const BaseExpressionRef * const m_leaves;
	const Expression * const m_expr;

public:
	inline ArraySlice(
        const BaseExpressionRef *leaves, const Expression *expr) :
        m_leaves(leaves), m_expr(expr) {
	}

	inline const BaseExpressionRef &operator[](size_t i) const {
		return m_leaves[i];
	}

	inline size_t size() const;

    inline const BaseExpressionRef *begin() const {
        return m_leaves;
    }

    inline const BaseExpressionRef *end() const {
        return m_leaves + size();
    }

	inline TypeMask type_mask() const;

	inline ExpressionRef clone(const BaseExpressionRef &head) const;

#if !COMPILE_TO_SLICE
    template<typename Generate>
    inline LeafVector create(const Generate &generate, size_t n) const {
        return sequential(generate, n).vector();
    }

    inline TypeMask exact_type_mask() const;

    template<typename T>
    class IndexToPrimitive {
    private:
        const BaseExpressionRef * const m_leaves;

    public:
        inline IndexToPrimitive(const BaseExpressionRef *leaves) : m_leaves(leaves) {
        }

        inline T convert(size_t i) const {
            return to_primitive<T>(m_leaves[i]);
        }
    };

    template<typename V>
    using PrimitiveCollection = PointerCollection<BaseExpressionRef, IndexToPrimitive<V>>;

    template<typename V>
    inline PrimitiveCollection<V> primitives() const {
        return PrimitiveCollection<V>(0, size(), IndexToPrimitive<V>(m_leaves));
    }

    inline auto leaves() const;
#endif
};
