#include "types.h"
#include "expression.h"
#include "pattern.h"
#include "evaluate.h"

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

	if (head->type() != SymbolType) {
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
		const Symbol *head_symbol = static_cast<const Symbol *>(head.get());

		return head_symbol->evaluate_with_head()(
			this,
			head,
			slice_code(),
			*_slice_ptr,
			evaluation);
	}
}

SymbolicFormRef Expression::instantiate_symbolic_form() const {
    return fast_symbolic_form(this);
}
