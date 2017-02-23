#pragma once

class VCallSliceIterator;

class VCallSlice {
private:
    const Expression * const m_expr;

    class Iterator {
    private:
        const Expression *m_expr;
        index_t m_index;

    public:
        inline Iterator() {
        }

        inline Iterator(const Iterator &iterator) :
            m_expr(iterator.m_expr), m_index(iterator.m_index) {
        }

        inline Iterator(const Expression *expr, size_t index) :
            m_expr(expr), m_index(index) {
        }

        inline Iterator &operator=(const Iterator &i) {
            m_expr = i.m_expr;
            m_index = i.m_index;
            return *this;
        }

        inline bool operator==(const Iterator &i) const {
            return m_expr == i.m_expr && m_index == i.m_index;
        }

        inline bool operator!=(const Iterator &i) const {
            return m_expr != i.m_expr || m_index != i.m_index;
        }

        inline bool operator>=(const Iterator &i) const {
            return m_index >= i.m_index;
        }

        inline bool operator>(const Iterator &i) const {
            return m_index > i.m_index;
        }

        inline bool operator<=(const Iterator &i) const {
            return m_index <= i.m_index;
        }

        inline bool operator<(const Iterator &i) const {
            return m_index < i.m_index;
        }

        inline BaseExpressionRef operator[](size_t i) const;

        inline BaseExpressionRef operator*() const;

        inline auto operator-(const Iterator &i) const {
            return m_index - i.m_index;
        }

        inline Iterator operator--(int) {
            Iterator previous(*this);
            --m_index;
            return previous;
        }

        inline Iterator &operator--() {
            --m_index;
            return *this;
        }

        inline Iterator operator++(int) {
            Iterator previous(*this);
            ++m_index;
            return previous;
        }

        inline Iterator &operator++() {
            ++m_index;
            return *this;
        }

        inline Iterator operator+(size_t i) const {
            return Iterator(m_expr, m_index + i);
        }

        inline Iterator &operator+=(size_t i) {
            m_index += i;
            return *this;
        }

        inline Iterator operator-(size_t i) const {
            return Iterator(m_expr, m_index - i);
        }

        inline Iterator &operator-=(size_t i) {
            m_index -= i;
            return *this;
        }
    };

public:
    inline VCallSlice(const Expression *expr) : m_expr(expr) {
    }

    inline const BaseExpressionRef operator[](size_t i) const;

    inline size_t size() const;

    inline Iterator begin() const {
        return Iterator(m_expr, 0);
    }

    inline Iterator end() const {
        return Iterator(m_expr, size());
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
        const Expression * const m_expr;

    public:
        inline IndexToPrimitive(const Expression *expr) : m_expr(expr) {
        }

        inline T convert(size_t i) const {
            return to_primitive<T>(m_expr->materialize_leaf(i));
        }
    };

    template<typename V>
    using PrimitiveCollection = PointerCollection<BaseExpressionRef, IndexToPrimitive<V>>;

    template<typename V>
    inline PrimitiveCollection<V> primitives() const {
        return PrimitiveCollection<V>(0, size(), IndexToPrimitive<V>(m_expr));
    }

    inline auto leaves() const {
        const ExpressionRef expr(m_expr);

        const auto access_leaf = [expr] (size_t i) {
            return expr->materialize_leaf(i);
        };

        return ByIndexCollection<decltype(access_leaf)>(access_leaf, size());
    }
#endif
};
