#include "mul.h"

inline bool is_plus(const BaseExpressionRef &expr) {
	if (expr->type() != ExpressionType) {
		return false;
	}

	return expr->as_expression()->head()->extended_type() == SymbolPlus;
}

inline bool times_number(
	const UnsafeBaseExpressionRef &number,
	std::vector<UnsafeBaseExpressionRef> &symbolics,
	UnsafeBaseExpressionRef &result,
	const Evaluation &evaluation) {

	if (number->type() != MachineIntegerType) {
		return false;
	}

	switch (static_cast<const MachineInteger*>(number.get())->value) {
		case 0:
			result = evaluation.zero;
			return true;

		case 1:
			result = expression(evaluation.Times, LeafVector(std::move(symbolics)));
			return true;

		case -1: {
			const BaseExpressionRef &plus = symbolics[0];

			if (is_plus(plus)) {
				symbolics[0] = plus->as_expression()->with_slice<CompileToSliceType>(
					[&evaluation] (const auto &slice) {
						return expression(evaluation.Plus, sequential([&slice, &evaluation] (auto &store) {
							const size_t n = slice.size();
							for (size_t i = 0; i < n; i++) {
								store(expression(evaluation.Times, evaluation.minus_one, slice[i]));
							}
						}, slice.size()));
					});

				result = expression(evaluation.Times, LeafVector(std::move(symbolics)));
				return true;
			} else {
				return false;
			}
		}

		default:
			return false;
	}
}

template<typename Slice>
inline BaseExpressionRef mul_slow(
	const Expression *expr,
	const Slice &slice,
	const Evaluation &evaluation) {

	std::vector<UnsafeBaseExpressionRef> symbolics;
	SymEngine::vec_basic numbers;

	for (const BaseExpressionRef leaf : slice.leaves()) {
		if (leaf->is_number()) {
			const auto form = symbolic_form(leaf);
			numbers.push_back(form->get());
		} else {
			symbolics.push_back(leaf);
		}
	}

	// it's important to check if a change happens. if we keep
	// returning non-null BaseExpressionRef here, "a * b" will
	// produce an infinite loop.

	if (symbolics.empty()) {
		if (numbers.size() >= 2) {
			return from_symbolic_form(
				SymEngine::mul(numbers),
				evaluation);
		} else {
			return BaseExpressionRef();
		}
	} else if (!numbers.empty()) {
		UnsafeBaseExpressionRef number;

		number = from_symbolic_form(
			SymEngine::mul(numbers),
			evaluation);

		UnsafeBaseExpressionRef result;

		if (times_number(number, symbolics, result, evaluation)) {
			return result;
		} else {
			if (numbers.size() >= 2) {
				symbolics.push_back(BaseExpressionRef(number));
				return expression(evaluation.Times, LeafVector(std::move(symbolics)));
			} else {
				return BaseExpressionRef();
			}
		}
	} else {
		return BaseExpressionRef();
	}
}

BaseExpressionRef mul(const Expression *expr, const Evaluation &evaluation) {
	// the most general and slowest form of mul
	return expr->with_slice<CompileToSliceType>([&expr, &evaluation] (const auto &slice) {
		return mul_slow(expr, slice, evaluation);
	});
};

BaseExpressionRef TimesNRule::try_apply(
	const Expression *expr, const Evaluation &evaluation) const {

	return expr->with_slice<CompileToSliceType>([&expr, &evaluation] (const auto &slice) {
		return mul_slow(expr, slice, evaluation);
	});
}
