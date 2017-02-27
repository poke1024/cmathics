#pragma once

#include "implementation.h"

inline BigExpressionRef expression(const BaseExpressionRef &head, BigSlice &&slice) {
    return BigExpression::construct(head, slice);
}

template<int N>
inline TinyExpressionRef<N> expression(const BaseExpressionRef &head, TinySlice<N> &&slice) {
    return TinyExpression<N>::construct(head, std::move(slice));
}

template<typename U>
inline PackedExpressionRef<U> expression(const BaseExpressionRef &head, PackedSlice<U> &&slice) {
    return ExpressionImplementation<PackedSlice<U>>::construct(head, slice);
}

template<typename E, typename T>
inline std::vector<T> collect(const LeafVector &leaves) {
    std::vector<T> values;
    values.reserve(leaves.size());
    for (const auto &leaf : leaves) {
        values.push_back(static_cast<const E*>(leaf.get())->value);
    }
    return values;
}

template<typename Type, typename PackedType>
inline ExpressionRef packed_expression(
    const BaseExpressionRef &head,
    LeafVector &&leaves) {

    return expression(head, PackedSlice<PackedType>(
        collect<Type, PackedType>(leaves)));
}

inline ExpressionRef non_tiny_expression(
    const BaseExpressionRef &head,
    LeafVector &&leaves) {

    if (leaves.size() < MinPackedSliceSize) {
        return expression(head, BigSlice(std::move(leaves)));
    } else {
        switch (leaves.type_mask()) {
            case make_type_mask(MachineIntegerType):
                return packed_expression<MachineInteger, machine_integer_t>(head, std::move(leaves));

            case make_type_mask(MachineRealType):
                return packed_expression<MachineReal, machine_real_t>(head, std::move(leaves));

            default:
                return expression(head, BigSlice(std::move(leaves)));
        }
    }
}

template<typename G>
typename std::enable_if<std::is_base_of<FGenerator, G>::value, ExpressionRef>::type
expression(
    const BaseExpressionRef &head,
    const G &generator) {

    if (generator.size() <= MaxTinySliceSize) {
        return tiny_expression(head, generator);
    } else {
        return non_tiny_expression(head, generator.vector());
    }
}

inline ExpressionRef
expression(
    const BaseExpressionRef &head,
    LeafVector &&leaves) {

    const size_t n = leaves.size();
    if (n <= MaxTinySliceSize) {
        return tiny_expression(head, sequential([&leaves, n] (auto &store) {
            for (size_t i = 0; i < n; i++) {
                store(leaves.unsafe_grab_leaf(i));
            }
        }, n));
    } else {
        return non_tiny_expression(head, std::move(leaves));
    }}


template<typename G>
typename std::enable_if<std::is_base_of<VGenerator, G>::value, ExpressionRef>::type
expression(
    const BaseExpressionRef &head,
    const G &generator) {

    return expression(head, generator.vector());
}

inline ExpressionRef expression(
    const BaseExpressionRef &head) {

    return TinyExpression<0>::construct(head);
}

inline ExpressionRef expression(
    const BaseExpressionRef &head,
    const BaseExpressionRef &a) {

    return expression(head, TinySlice<1>({a}));
}

inline ExpressionRef expression(
    const BaseExpressionRef &head,
    const BaseExpressionRef &a,
    const BaseExpressionRef &b) {

    return expression(head, TinySlice<2>({a, b}));
}

inline ExpressionRef expression(
    const BaseExpressionRef &head,
    const BaseExpressionRef &a,
    const BaseExpressionRef &b,
    const BaseExpressionRef &c) {

    return expression(head, TinySlice<3>({a, b, c}));
}

inline ExpressionRef expression(
    const BaseExpressionRef &head,
    const BaseExpressionRef &a,
    const BaseExpressionRef &b,
    const BaseExpressionRef &c,
    const BaseExpressionRef &d) {

    return expression(head, TinySlice<4>({a, b, c, d}));
}

inline ExpressionRef expression(
    const BaseExpressionRef &head,
    const std::initializer_list<BaseExpressionRef> &leaves) {

    if (leaves.size() <= MaxTinySliceSize) {
        return tiny_expression(head, sequential([&leaves] (auto &store) {
            for (const BaseExpressionRef &leaf : leaves) {
                store(BaseExpressionRef(leaf));
            }
        }, leaves.size()));
    } else {
        return expression(head, BigSlice(leaves, UnknownTypeMask));
    }
}

inline ExpressionRef expression(const BaseExpressionRef &head, const ArraySlice &slice) {
    return slice.clone(head);
}

inline ExpressionRef expression(const BaseExpressionRef &head, const VCallSlice &slice) {
    return slice.clone(head);
}
