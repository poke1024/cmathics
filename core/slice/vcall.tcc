#pragma once

inline BaseExpressionRef VCallSlice::Iterator::operator[](size_t i) const {
    return m_expr->materialize_leaf(m_index + i);
}

inline BaseExpressionRef VCallSlice::Iterator::operator*() const {
    return m_expr->materialize_leaf(m_index);
}

inline const BaseExpressionRef VCallSlice::operator[](size_t i) const {
    return m_expr->materialize_leaf(i);
}

inline size_t VCallSlice::size() const {
    return m_expr->size();
}

inline TypeMask VCallSlice::type_mask() const {
    return m_expr->materialize_type_mask();
}

inline ExpressionRef VCallSlice::clone(const BaseExpressionRef &head) const {
    return m_expr->clone(head);
}

#if !COMPILE_TO_SLICE
inline TypeMask VCallSlice::exact_type_mask() const {
    return m_expr->materialize_exact_type_mask();
}
#endif
