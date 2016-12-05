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
	_hold_none.initialize<_HoldNone>();
	_hold_first.initialize<_HoldFirst>();
	_hold_rest.initialize<_HoldRest>();
	_hold_all.initialize<_HoldAll>();
	_hold_all_complete.initialize<_HoldAllComplete>();
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

boost::object_pool<generic_adder> generic_adders;
boost::object_pool<integer_adder> integer_adders;
boost::object_pool<initial_adder> initial_adders;

packable_heap_storage::packable_heap_storage(size_t size) {
	m_adder = initial_adders.construct(size);
}

void generic_adder::release() {
	generic_adders.free(this);
}

adder *integer_adder::add(const BaseExpressionRef &expr) {
	if (expr->type() == MachineIntegerType) {
		m_integers.push_back(static_cast<const MachineInteger*>(expr.get())->value);
		return this;
	} else {
		std::vector<BaseExpressionRef> leaves;
		leaves.reserve(m_size);
		for (const auto x : m_integers) {
			leaves.emplace_back(Heap::MachineInteger(x));
		}
		integer_adders.free(this);
		generic_adder *new_adder = generic_adders.construct(
			leaves, MakeTypeMask(MachineIntegerType));
		new_adder->add(expr);
		return new_adder;
	}
}

void integer_adder::release() {
	integer_adders.free(this);
}

adder *initial_adder::add(const BaseExpressionRef &expr) {
	switch (expr->type()) {
		case MachineIntegerType: {
			integer_adder *new_adder = integer_adders.construct(expr, m_size);
			initial_adders.free(this);
			return new_adder;
		}
		default: {
			generic_adder *new_adder = generic_adders.construct(expr, m_size);
			initial_adders.free(this);
			return new_adder;
		}
	}
}

void initial_adder::release() {
	initial_adders.free(this);
}
