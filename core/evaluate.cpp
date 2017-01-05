#include "types.h"
#include "expression.h"
#include "evaluate.h"
#include "definitions.h"
#include "matcher.h"

EvaluateDispatch *EvaluateDispatch::s_instance = nullptr;

void EvaluateDispatch::init() {
	assert(s_instance == nullptr);
	s_instance = new EvaluateDispatch();
}

const Evaluate *EvaluateDispatch::pick(Attributes attributes) {
	switch (attributes_bitmask_t(attributes)) {
		case attributes_bitmask_t(Attributes::None):
			return s_instance->m_none.vtable();

		case attributes_bitmask_t(Attributes::HoldFirst):
			assert(!(attributes & Attributes::HoldAllComplete));
			return s_instance->m_hold_first.vtable();

		case attributes_bitmask_t(Attributes::HoldAll):
			assert(!(attributes & Attributes::HoldAllComplete));
			return s_instance->m_hold_all.vtable();

		case attributes_bitmask_t(Attributes::HoldRest):
			assert(!(attributes & Attributes::HoldAllComplete));
			return s_instance->m_hold_rest.vtable();

		case attributes_bitmask_t(Attributes::HoldAllComplete):
			return s_instance->m_hold_all_complete.vtable();

		case attributes_bitmask_t(Attributes::Listable + Attributes::NumericFunction):
			return s_instance->m_listable_numeric_function.vtable();

		default:
			return s_instance->m_dynamic.vtable();
	}
}
