#include "types.h"
#include "core/expression/implementation.h"
#include "evaluate.h"
#include "definitions.h"
#include "core/pattern/matcher.tcc"

EvaluateDispatch *EvaluateDispatch::s_instance = nullptr;

typedef Slice GenericSlice;

template<typename IntermediateExpression, typename Slice, typename ReducedAttributes>
BaseExpressionRef evaluate_intermediate_form(
    const IntermediateExpression *expr,
    const Slice &slice,
    const ReducedAttributes &attributes,
    const Evaluation &evaluation) {

    if (attributes & Attributes::Listable) {
        bool done;
        UnsafeExpressionRef threaded;
        std::tie(done, threaded) = expr->thread(evaluation);
        if (done) {
            return threaded;
        }
    }

    // Step 3
    // Apply UpValues for leaves

    if ((attributes & Attributes::HoldAllComplete) == 0 &&
        slice.type_mask() & make_type_mask(SymbolType, ExpressionType)) {

        const size_t n = slice.size();
        for (size_t i = 0; i < n; i++) {
            // FIXME do not check symbols twice
            const Symbol * const up_name = slice[i]->lookup_name();
            if (up_name == nullptr) {
                continue;
            }
            const SymbolRules *const up_rules = up_name->state().rules();
            if (up_rules) {
                const optional<BaseExpressionRef> up_form = up_rules->up_rules.apply(
                    expr, evaluation);
                if (up_form) {
                    return *up_form;
                }
            }
        }
    }

    assert(expr->head()->is_symbol());
    const Symbol * const head_symbol = static_cast<const Symbol*>(expr->head());

    const SymbolRules * const rules = head_symbol->state().rules();

    if (rules) {
        // Step 4
        // Evaluate the head with leaves. (DownValue)

        const optional<BaseExpressionRef> down_form = rules->down_rules.apply(
            expr, evaluation);
        if (down_form) {
            return *down_form;
        }
    }

    return BaseExpressionRef();
}

template<typename Slice, typename ReducedAttributes>
BaseExpressionRef evaluate(
    const Expression *generic_self,
    const BaseExpressionRef &head,
    const GenericSlice &generic_slice,
    const Attributes full_attributes,
    const Evaluation &evaluation) {

    using Implementation = const ExpressionImplementation<Slice>;

    const Implementation * const self =
		static_cast<const ExpressionImplementation<Slice>*>(generic_self);

    const Slice &slice = static_cast<const Slice&>(generic_slice);

    const ReducedAttributes attributes(full_attributes);
	const size_t n = slice.size();

	const auto &evaluate_leaves = [self, &head, &slice, &evaluation]
		(size_t begin, size_t end, const auto &hold) {
			return conditional_map_indexed<ExpressionType, SymbolType>(
				head,
				head != self->_head,
				lambda([&hold, &evaluation] (size_t i, const BaseExpressionRef &leaf) {
					if (hold(i)) {
						if (leaf->has_form(S::Evaluate, 1, evaluation)) {
							return leaf->evaluate(evaluation);
						} else {
							return BaseExpressionRef();
						}
					} else {
						if (!leaf->has_form(S::Unevaluated, 1, evaluation)) {
							return leaf->evaluate(evaluation);
						} else {
							return BaseExpressionRef();
						}
					}
				}),
				slice,
				begin,
				end,
				evaluation);
		};

	UnsafeExpressionRef intermediate_form;

    if (attributes & Attributes::HoldAllComplete) { // HoldAllComplete
	    intermediate_form = evaluate_leaves(0, 0, [] (size_t i) {
		    return false;
	    });
    } else if (attributes & Attributes::HoldFirst) {
        if (attributes & Attributes::HoldRest) { // i.e. HoldAll
            // TODO flatten sequences
	        intermediate_form = evaluate_leaves(0, n, [] (size_t i) {
		        return true;
	        });
        } else {
	        intermediate_form = evaluate_leaves(0, n, [] (size_t i) {
		        return i < 1;
	        });
        }
    } else if (attributes & Attributes::HoldRest) {
	    intermediate_form = evaluate_leaves(0, n, [] (size_t i) {
		    return i > 0;
	    });
    } else {
	    intermediate_form = evaluate_leaves(0, n, [] (size_t i) {
		    return false;
	    });
    }

#if 0
    std::cout
        << "EVALUATED " << self
        << " AT [" << eval_leaf.first << ", " << eval_leaf.second << "]"
        << " TO " << intermediate_form << std::endl;
#endif

    const auto evaluate_unknown_size = [&attributes, &evaluation] (ExpressionPtr expr) {
        return coalesce(expr->with_slice_c(
            [expr, &attributes, &evaluation] (const auto &slice) {
                return evaluate_intermediate_form(
                    expr,
                    slice,
                    attributes,
                    evaluation);
            }), expr);
    };

    const bool should_flatten_sequence =
        (attributes & (Attributes::SequenceHold + Attributes::HoldAllComplete)) == 0;

    if (intermediate_form) {
        if (should_flatten_sequence) {
            const ExpressionRef flattened = intermediate_form->flatten_sequence();
            if (flattened) {
                return evaluate_unknown_size(flattened.get());
            }
        }

        if (is_tiny_slice(Slice::code())) { // tiny slices always produce tiny slices
            assert(intermediate_form->slice_code() == Slice::code());
            const auto *expr = static_cast<const Implementation*>(intermediate_form.get());

            return coalesce(evaluate_intermediate_form(
                expr,
                expr->slice(),
                attributes,
                evaluation), intermediate_form);
        } else { // non-tiny slices might produce a variety of slice types; we need to check
            return evaluate_unknown_size(intermediate_form.get());
        }
    } else {
        if (should_flatten_sequence) {
            const ExpressionRef flattened = self->flatten_sequence();
            if (flattened) {
                return evaluate_unknown_size(flattened.get());
            }
        }

        return evaluate_intermediate_form(
            self, slice, attributes, evaluation);
    }
}

template<Attributes fixed_attributes>
class FixedAttributes {
public:
    inline FixedAttributes(Attributes attributes) {
    }

    inline constexpr bool
    operator&(Attributes y) const {
        return (attributes_bitmask_t(fixed_attributes) & attributes_bitmask_t(y)) != 0;
    }
};

template<typename ReducedAttributes>
template<int N>
void EvaluateDispatch::Precompiled<ReducedAttributes>::initialize_static_slice() {
    m_vtable.entry[TinySlice0Code + N] = ::evaluate<TinySlice<N>, ReducedAttributes>;

    STATIC_IF (N >= 1) {
        initialize_static_slice<N - 1>();
    } STATIC_ENDIF
}

template<typename ReducedAttributes>
EvaluateDispatch::Precompiled<ReducedAttributes>::Precompiled() {
    static_assert(1 + PackedSliceMachineRealCode - TinySlice0Code ==
        NumberOfSliceCodes, "slice code ids error");

    m_vtable.entry[BigSliceCode] = ::evaluate<BigSlice, ReducedAttributes>;

    m_vtable.entry[PackedSliceMachineIntegerCode] =
        ::evaluate<PackedSlice<machine_integer_t>, ReducedAttributes>;
    m_vtable.entry[PackedSliceMachineRealCode] =
        ::evaluate<PackedSlice<machine_real_t>, ReducedAttributes>;

    initialize_static_slice<MaxTinySliceSize>();
}

EvaluateDispatch::EvaluateDispatch() {
    static Precompiled<FixedAttributes<Attributes::None>> none;
    static Precompiled<FixedAttributes<Attributes::HoldFirst>> hold_first;
    static Precompiled<FixedAttributes<Attributes::HoldRest>> hold_rest;
    static Precompiled<FixedAttributes<Attributes::HoldAll>> hold_all;
    static Precompiled<FixedAttributes<Attributes::HoldAllComplete>> hold_all_complete;
    static Precompiled<FixedAttributes<Attributes::Listable + Attributes::NumericFunction>> listable_numeric_function;
    static Precompiled<Attributes> dynamic;

    m_evaluate[PrecompiledNone] = none.vtable();
    m_evaluate[PrecompiledHoldFirst] = hold_first.vtable();
    m_evaluate[PrecompiledHoldRest] = hold_rest.vtable();
    m_evaluate[PrecompiledHoldAll] = hold_all.vtable();
    m_evaluate[PrecompiledHoldAllComplete] = hold_all_complete.vtable();
    m_evaluate[PrecompiledListableNumericFunction] = listable_numeric_function.vtable();
    m_evaluate[PrecompiledDynamic] = dynamic.vtable();
}

void EvaluateDispatch::init() {
	assert(s_instance == nullptr);
	s_instance = new EvaluateDispatch();
}

DispatchableAttributes EvaluateDispatch::pick(Attributes attributes) {
    int precompiled_code;

	switch (attributes_bitmask_t(attributes)) {
		case attributes_bitmask_t(Attributes::None):
            precompiled_code = PrecompiledNone;
            break;

		case attributes_bitmask_t(Attributes::HoldFirst):
			assert(!(attributes & Attributes::HoldAllComplete));
            precompiled_code = PrecompiledHoldFirst;
            break;

		case attributes_bitmask_t(Attributes::HoldAll):
			assert(!(attributes & Attributes::HoldAllComplete));
            precompiled_code = PrecompiledHoldAll;
            break;

		case attributes_bitmask_t(Attributes::HoldRest):
			assert(!(attributes & Attributes::HoldAllComplete));
            precompiled_code = PrecompiledHoldRest;
            break;

		case attributes_bitmask_t(Attributes::HoldAllComplete):
            precompiled_code = PrecompiledHoldAllComplete;
            break;

		case attributes_bitmask_t(Attributes::Listable + Attributes::NumericFunction):
            precompiled_code = PrecompiledListableNumericFunction;
            break;

		default:
            precompiled_code = PrecompiledDynamic;
            break;
	}

    return precompiled_code | (uint64_t(attributes) << 8);
}
