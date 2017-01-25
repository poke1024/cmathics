#include "add.h"

template<typename Slice>
inline BaseExpressionRef add_only_integers(const Slice &slice) {
	// sums an all MachineInteger/BigInteger expression

	Numeric::Z result(0);

	for (auto value : slice.template primitives<Numeric::Z>()) {
		result += value;
	}

	return result.to_expression();
}

template<typename Slice>
inline BaseExpressionRef add_only_machine_reals(const Slice &slice) {
	// sums an all MachineReal expression

	machine_real_t result = 0.;

	for (machine_real_t value : slice.template primitives<machine_real_t>()) {
		result += value;
	}

	return Pool::MachineReal(result);
}

template<typename Slice>
inline BaseExpressionRef add_machine_inexact(
	const Expression *expr,
	const Slice &slice,
	const Evaluation &evaluation) {

	// create an array to store all the symbolic arguments which can't be evaluated.

	LeafVector symbolics;
	SymEngine::vec_basic symengine;

	machine_real_t sum = 0.0;

	for (const BaseExpressionRef leaf : slice.leaves()) {
		const BaseExpression *leaf_ptr = leaf.get();

		const auto type = leaf_ptr->type();
		switch (type) {
			case MachineIntegerType:
				sum += machine_real_t(static_cast<const MachineInteger*>(leaf_ptr)->value);
				break;
			case BigIntegerType:
				sum += static_cast<const BigInteger*>(leaf_ptr)->value.get_d();
				break;

			case MachineRealType:
				sum += static_cast<const MachineReal*>(leaf_ptr)->value;
				break;
			case BigRealType:
				sum += static_cast<const BigReal*>(leaf_ptr)->as_double();
				break;

			case BigRationalType:
				sum += static_cast<const BigRational*>(leaf_ptr)->value.get_d();
				break;

			case MachineRationalType:
			case MachineComplexType:
			case BigComplexType:
				symengine.push_back(symbolic_form(leaf, evaluation)->get());
				break;

			default:
				symbolics.push_back_copy(leaf);
				break;
		}
	}

	// we expect at least one non-symbolic
	assert(symbolics.size() != slice.size());

	UnsafeBaseExpressionRef result;

	if (symbolics.size() == slice.size() - 1) {
		// one non-symbolic: nothing to do; return identity result
	} else {
		if (!symengine.empty()) {
			static_assert(sizeof(double) == sizeof(machine_real_t),
                "machine_real_t is expected to be a double here");

			symengine.push_back(SymEngine::real_double(sum));

			result = from_symbolic_form(
				SymEngine::add(symengine),
				evaluation);
		} else {
			result = from_primitive(sum);
		}

		if (!symbolics.empty()) {
			// at least one symbolic
			symbolics.push_back(BaseExpressionRef(result));
			result = expression(expr->head(), std::move(symbolics));
		}
	}

	return result;
}

template<typename Slice>
inline BaseExpressionRef add_slow(
	const Expression *expr,
	const Slice &slice,
	const Evaluation &evaluation) {

	LeafVector symbolics;
	SymEngine::vec_basic numbers;

	for (const BaseExpressionRef leaf : slice.leaves()) {
		if (leaf->is_number()) {
			const auto form = symbolic_form(leaf, evaluation);
			numbers.push_back(form->get());
		} else {
			symbolics.push_back_copy(leaf);
		}
	}

	// it's important to check if a change happens. if we keep
	// returning non-null BaseExpressionRef here, "a + b" will
	// produce an infinite loop.

	if (numbers.size() >= 2) {
		UnsafeBaseExpressionRef result;

		result = from_symbolic_form(
			SymEngine::add(numbers),
			evaluation);

		if (!symbolics.empty()) {
			symbolics.push_back(BaseExpressionRef(result));
			result = expression(expr->head(), std::move(symbolics));
		}

		return result;
	} else {
		return BaseExpressionRef();
	}
}

BaseExpressionRef add(const Expression *expr, const Evaluation &evaluation) {
	// the most general and slowest form of add
	return expr->with_slice<CompileToSliceType>([&expr, &evaluation] (const auto &slice) {
		return add_slow(expr, slice, evaluation);
	});
};

BaseExpressionRef PlusNRule::try_apply(
	const Expression *expr, const Evaluation &evaluation) const {

	// we guarantee that expr.size() >= 3 through the given match_size()
	// in the corresponding Rule.
	assert(expr->size() >= 3);

	return expr->with_slice<CompileToSliceType>([&expr, &evaluation] (const auto &slice) {
		// bit field to determine which types are present
		const TypeMask types_seen = slice.exact_type_mask();

		// expression is all MachineReals.
		if (types_seen == make_type_mask(MachineRealType)) {
			return add_only_machine_reals(slice);
		}

		constexpr TypeMask int_mask =
			make_type_mask(BigIntegerType) | make_type_mask(MachineIntegerType);

		// expression is all Integers.
		if ((types_seen & int_mask) == types_seen) {
			return add_only_integers(slice);
		}

		// expression contains a Real.
		if (types_seen & make_type_mask(MachineRealType)) {
			return add_machine_inexact(expr, slice, evaluation);
		}

		// ask SymEngine for help.
		return add_slow(expr, slice, evaluation);
	});
}