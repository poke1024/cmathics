#include "mul.h"

inline bool is_plus(const BaseExpressionRef &expr) {
	if (!expr->is_expression()) {
		return false;
	}

	return expr->as_expression()->head()->symbol() == S::Plus;
}

inline bool times_number(
	const BaseExpressionRef &number,
	std::vector<BaseExpressionRef> &&symbolics,
	UnsafeBaseExpressionRef &result,
	const Evaluation &evaluation) {

	if (!number->is_machine_integer()) {
		return false;
	}

	switch (static_cast<const MachineInteger*>(number.get())->value) {
		case 0:
			result = evaluation.definitions.zero;
			return true;

		case 1:
			result = expression(evaluation.Times, LeafVector(std::move(symbolics)));
			return true;

		case -1: {
			const BaseExpressionRef &plus = symbolics[0];

			if (is_plus(plus)) {
				symbolics[0].unsafe_mutate(plus->as_expression()->with_slice_c(
					[&evaluation] (const auto &slice) {
						return expression(evaluation.Plus, sequential([&slice, &evaluation] (auto &store) {
							const size_t n = slice.size();
							for (size_t i = 0; i < n; i++) {
								store(expression(evaluation.Times, evaluation.definitions.minus_one, slice[i]));
							}
						}, slice.size()));
					}));

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

	if (expr->is_symbolic_form_evaluated()) {
		return BaseExpressionRef();
	}

	try {
		SymEngine::vec_basic numbers;
		std::vector<BaseExpressionRef> rest;

		for (const BaseExpressionRef leaf : slice.leaves()) {
			if (leaf->is_number()) {
				const auto form = symbolic_form(leaf, evaluation);
				numbers.push_back(form->get());
			} else {
				rest.push_back(leaf);
			}
		}

		// carefully check if a change happens. if we keep returning
		// non-null BaseExpressionRef here, an expression like "a * b"
		// will trigger an infinite loop.

		SymEngine::vec_basic operands;

		if (numbers.size() > 0) {
			const auto number = SymEngine::mul(numbers);

			const BaseExpressionRef number_expression =
				from_symbolic_form(number, evaluation);

			UnsafeBaseExpressionRef result;

			if (times_number(number_expression, std::move(rest), result, evaluation)) {
				return result;
			} else {
				operands.push_back(number);
			}
		}

		LeafVector new_rest;

		for (const BaseExpressionRef &leaf : rest) {
			const auto form = symbolic_form(leaf, evaluation);
			if (!form->is_none()) {
				operands.push_back(form->get());
			} else {
				new_rest.push_back_copy(leaf);
			}
		}

		const auto multiplied = SymEngine::mul(operands);
		const bool is_active_form = new_rest.empty();

		if (operands.size() >= 1) {
			if (multiplied->get_type_code() == SymEngine::MUL) {
				const auto &args = multiplied->get_args();

				new_rest.reserve(new_rest.size() + args.size());
				for (size_t i = 0; i < args.size(); i++) {
					new_rest.push_back(from_symbolic_form(args[i], evaluation));
				}
			} else {
				new_rest.push_back(from_symbolic_form(multiplied, evaluation));
			}
		}

		new_rest.sort();

		const BaseExpressionRef result =
			expression(evaluation.Times, std::move(new_rest));

		if (result->same(expr)) {
			if (is_active_form) {
				expr->set_symbolic_form(multiplied);
			} else {
				expr->set_no_symbolic_form(evaluation);
			}
			return BaseExpressionRef();
		} else {
			if (is_active_form) {
				result->set_symbolic_form(multiplied);
			} else {
				result->set_no_symbolic_form(evaluation);
			}
			return result;
		}
	} catch (const SymEngine::SymEngineException &e) {
		evaluation.sym_engine_exception(e);
		return BaseExpressionRef();
	}
}

BaseExpressionRef mul(const Expression *expr, const Evaluation &evaluation) {
	// the most general and slowest form of mul
	return expr->with_slice_c([expr, &evaluation] (const auto &slice) {
		return mul_slow(expr, slice, evaluation);
	});
};

optional<BaseExpressionRef> TimesNRule::try_apply(
	const Expression *expr,
    const Evaluation &evaluation) const {

	return expr->with_slice_c([expr, &evaluation] (const auto &slice) {
		return mul_slow(expr, slice, evaluation);
	});
}
