#include "types.h"
#include "expression.h"
#include "evaluate.h"
#include "definitions.h"
#include "matcher.h"

EvaluateDispatch *EvaluateDispatch::s_instance = nullptr;

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
