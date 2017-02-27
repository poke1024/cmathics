#include "core/types.h"
#include "implementation.h"
#include "core/pattern/arguments.h"
#include "core/evaluate.h"

BaseExpressionRef Expression::evaluate_expression(
	const Evaluation &evaluation) const {

	// Evaluate the head

	UnsafeBaseExpressionRef head = _head;

	while (true) {
		auto new_head = head->evaluate(evaluation);
		if (new_head) {
			head = new_head;
		} else {
			break;
		}
	}

	// Evaluate the leaves and apply rules.

	if (!head->is_symbol()) {
		if (head.get() != _head.get()) {
			const ExpressionRef new_head_expr = static_pointer_cast<const Expression>(clone(head));
			const BaseExpressionRef result = new_head_expr->evaluate_expression_with_non_symbol_head(evaluation);
			if (result) {
				return result;
			} else {
				return new_head_expr;
			}
		} else {
			return evaluate_expression_with_non_symbol_head(evaluation);
		}
	} else {
		const Symbol *head_symbol = head->as_symbol();

		return head_symbol->state().dispatch(
			this, slice_code(), *_slice_ptr, evaluation);
	}
}

SymbolicFormRef Expression::instantiate_symbolic_form(const Evaluation &evaluation) const {
    return evaluation.no_symbolic_form;
}
