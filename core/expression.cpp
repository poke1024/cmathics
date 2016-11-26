#include "types.h"
#include "expression.h"
#include "pattern.h"
#include "evaluate.h"

BaseExpressionRef Expression::evaluate_expression(
	const BaseExpressionRef &self, const Evaluation &evaluation) const {

	// Evaluate the head

	auto head = _head;

	while (true) {
		auto new_head = head->evaluate(head, evaluation);
		if (new_head) {
			head = new_head;
		} else {
			break;
		}
	}

	// Evaluate the leaves and apply rules.

	if (head->type() != SymbolType) {
		if (head != _head) {
			ExpressionRef new_head_expr = boost::static_pointer_cast<const Expression>(clone(head));
			BaseExpressionRef result = new_head_expr->evaluate_expression_with_non_symbol_head(
				new_head_expr, evaluation);
			if (result) {
				return result;
			} else {
				return new_head_expr;
			}
		} else {
			return evaluate_expression_with_non_symbol_head(
				boost::static_pointer_cast<const Expression>(self), evaluation);
		}
	} else {
		const Symbol *head_symbol = static_cast<const Symbol *>(head.get());

		return head_symbol->evaluate_with_head()(
			boost::static_pointer_cast<const Expression>(self),
			head,
			slice_code(),
			*_slice_ptr,
			evaluation);
	}
}
