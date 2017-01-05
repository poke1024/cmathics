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

EvaluateDispatch::EvaluateDispatch() {
	_hold_none.initialize<_HoldNone, _NoAttributes>();
	_hold_first.initialize<_HoldFirst, _NoAttributes>();
	_hold_rest.initialize<_HoldRest, _NoAttributes>();
	_hold_all.initialize<_HoldAll, _NoAttributes>();
	_hold_all_complete.initialize<_HoldAllComplete, _NoAttributes>();
}

const Evaluate *EvaluateDispatch::pick(Attributes attributes) {
	if (attributes & Attributes::HoldFirst) {
		assert(!(attributes & Attributes::HoldAllComplete));
		if (attributes & Attributes::HoldRest) {
			return &s_instance->_hold_all;
		} else {
			return &s_instance->_hold_first;
		}
	} else if (attributes & Attributes::HoldRest) {
		assert(!(attributes & Attributes::HoldAllComplete));
		return &s_instance->_hold_rest;
	} else if (attributes & Attributes::HoldAllComplete) {
		return &s_instance->_hold_all_complete;
	} else {
		return &s_instance->_hold_none;
	}
}
