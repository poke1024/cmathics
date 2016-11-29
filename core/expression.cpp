#include "types.h"
#include "expression.h"
#include "pattern.h"
#include "evaluate.h"

BaseExpressionRef Expression::evaluate_expression(
	const Evaluation &evaluation) const {

	// Evaluate the head

	auto head = _head;

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
		if (head != _head) {
			const ExpressionRef new_head_expr = boost::static_pointer_cast<const Expression>(clone(head));
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


typedef SymEngine::RCP<const SymEngine::Basic> (*SymEngineUnaryFunction)(
		const SymEngine::RCP<const SymEngine::Basic>&);

typedef SymEngine::RCP<const SymEngine::Basic> (*SymEngineBinaryFunction)(
		const SymEngine::RCP<const SymEngine::Basic>&,
		const SymEngine::RCP<const SymEngine::Basic>&);

typedef SymEngine::RCP<const SymEngine::Basic> (*SymEngineNAryFunction)(
		const SymEngine::vec_basic&);

inline SymbolicForm symbolic_1(const SymEngineUnaryFunction &f, const Expression *expr) {
	const BaseExpressionRef *leaves = expr->static_leaves<1>();
	SymbolicForm symbolic_a = leaves[0]->symbolic_form();
	if (!symbolic_a.is_null()) {
		return f(symbolic_a);
	}
	return SymbolicForm();
}

inline SymbolicForm symbolic_2(const SymEngineBinaryFunction &f, const Expression *expr) {
	const BaseExpressionRef *leaves = expr->static_leaves<2>();
	SymbolicForm symbolic_a = leaves[0]->symbolic_form();
	if (!symbolic_a.is_null()) {
		SymbolicForm symbolic_b = leaves[1]->symbolic_form();
		if (!symbolic_b.is_null()) {
			return f(symbolic_a, symbolic_b);
		}
	}
	return SymbolicForm();
}

inline SymbolicForm symbolic_n(const SymEngineNAryFunction &f, const Expression *expr) {
	const optional<SymEngine::vec_basic> operands = expr->symbolic_operands();
	if (operands) {
		return f(*operands);
	} else {
		return SymbolicForm();
	}
}


