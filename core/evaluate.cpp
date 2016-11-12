#include "types.h"
#include "expression.h"
#include "evaluate.h"

EvaluateDispatch *EvaluateDispatch::s_instance = nullptr;

void EvaluateDispatch::init() {
	assert(s_instance == nullptr);
	s_instance = new EvaluateDispatch();
}

EvaluateDispatch::EvaluateDispatch() {
	_hold_none.fill<_HoldNone>();
	_hold_first.fill<_HoldFirst>();
	_hold_rest.fill<_HoldRest>();
	_hold_all.fill<_HoldAll>();
	_hold_all_complete.fill<_HoldAllComplete>();
}

const Evaluate &EvaluateDispatch::pick(Attributes attributes) {
	// only one type of Hold attribute can be set at a time
	assert(count(
		attributes,
		Attributes::HoldFirst +
		Attributes::HoldRest +
		Attributes::HoldAll +
		Attributes::HoldAllComplete) <= 1);

	if (attributes & Attributes::HoldFirst) {
		return s_instance->_hold_first;
	} else if (attributes & Attributes::HoldRest) {
		return s_instance->_hold_rest;
	} else if (attributes & Attributes::HoldAll) {
		return s_instance->_hold_all;
	} else if (attributes & Attributes::HoldAllComplete) {
		return s_instance->_hold_all_complete;
	} else {
		return s_instance->_hold_none;
	}
}
