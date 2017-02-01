#include "types.h"
#include "expression.h"
#include "evaluate.h"
#include "definitions.h"
#include "matcher.h"

EvaluateDispatch *EvaluateDispatch::s_instance = nullptr;

typedef std::pair<size_t, size_t> eval_range;

typedef Slice GenericSlice;

template<typename Slice, typename ReducedAttributes>
BaseExpressionRef evaluate(
    const Expression *self,
    const BaseExpressionRef &head,
    const GenericSlice &generic_slice,
    const Attributes full_attributes,
    const Evaluation &evaluation) {

    const Slice &slice = static_cast<const Slice&>(generic_slice);
    eval_range eval_leaf;

    const ReducedAttributes attributes(full_attributes);

    if (attributes & Attributes::HoldAllComplete) { // HoldAllComplete
        eval_leaf = eval_range(0, 0);
    } else if (attributes & Attributes::HoldFirst) {
        if (attributes & Attributes::HoldRest) { // i.e. HoldAll
            // TODO flatten sequences
            eval_leaf = eval_range(0, 0);
        } else {
            eval_leaf = eval_range(1, slice.size());
        }
    } else if (attributes & Attributes::HoldRest) {
        eval_leaf = eval_range(0, 1);
    } else {
        eval_leaf = eval_range(0, slice.size());
    }

    const Symbol * const head_symbol = static_cast<const Symbol*>(head.get());

    assert(0 <= eval_leaf.first);
    assert(eval_leaf.first <= eval_leaf.second);
    assert(eval_leaf.second <= slice.size());

    const ExpressionRef intermediate_form =
        conditional_map<ExpressionType,SymbolType>(
            head,
            head != self->_head,
            lambda([&evaluation] (const BaseExpressionRef &leaf) {
                return leaf->evaluate(evaluation);
            }),
            slice,
            eval_leaf.first,
            eval_leaf.second,
            evaluation);

#if 0
    std::cout
        << "EVALUATED " << self
        << " AT [" << eval_leaf.first << ", " << eval_leaf.second << "]"
        << " TO " << intermediate_form << std::endl;
#endif

    const Expression * const safe_intermediate_form =
        intermediate_form ?
        intermediate_form.get() :
        self;

    if (attributes & Attributes::Listable) {
        bool done;
        UnsafeExpressionRef threaded;
        std::tie(done, threaded) = safe_intermediate_form->thread(evaluation);
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
            const Symbol * const up_name = lookup_name(slice[i].get());
            if (up_name == nullptr) {
                continue;
            }
            const SymbolRules *const up_rules = up_name->state().rules();
            if (up_rules) {
                const BaseExpressionRef up_form = up_rules->up_rules.try_and_apply<Slice>(
                    safe_intermediate_form, evaluation);
                if (up_form) {
                    return up_form;
                }
            }
        }
    }

    const SymbolRules * const rules = head_symbol->state().rules();

    if (rules) {
        // Step 4
        // Evaluate the head with leaves. (DownValue)

        const BaseExpressionRef down_form = rules->down_rules.try_and_apply<Slice>(
           safe_intermediate_form, evaluation);
        if (down_form) {
            return down_form;
        }
    }

    return intermediate_form;
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
    m_vtable.entry[StaticSlice0Code + N] = ::evaluate<StaticSlice<N>, ReducedAttributes>;

    STATIC_IF (N >= 1) {
        initialize_static_slice<N - 1>();
    } STATIC_ENDIF
}

template<typename ReducedAttributes>
EvaluateDispatch::Precompiled<ReducedAttributes>::Precompiled() {
    static_assert(1 + PackedSliceMachineRealCode - StaticSlice0Code ==
        NumberOfSliceCodes, "slice code ids error");

    m_vtable.entry[DynamicSliceCode] = ::evaluate<DynamicSlice, ReducedAttributes>;

    m_vtable.entry[PackedSliceMachineIntegerCode] =
        ::evaluate<PackedSlice<machine_integer_t>, ReducedAttributes>;
    m_vtable.entry[PackedSliceMachineRealCode] =
        ::evaluate<PackedSlice<machine_real_t>, ReducedAttributes>;

    initialize_static_slice<MaxStaticSliceSize>();
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
