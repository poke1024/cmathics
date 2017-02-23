#pragma once

inline size_t ArraySlice::size() const {
    return m_expr->size();
}

inline TypeMask ArraySlice::type_mask() const {
    return m_expr->materialize_type_mask();
}

inline ExpressionRef ArraySlice::clone(const BaseExpressionRef &head) const {
    return m_expr->clone(head);
}

#if !COMPILE_TO_SLICE
inline TypeMask ArraySlice::exact_type_mask() const {
    return m_expr->materialize_exact_type_mask();
}

inline auto ArraySlice::leaves() const {
    const ExpressionRef expr(m_expr);

    const auto access_leaf = [expr] (size_t i) {
        return expr->materialize_leaf(i);
    };

    return ByIndexCollection<decltype(access_leaf)>(access_leaf, size());
}
#endif
