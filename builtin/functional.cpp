#include "functional.h"

// as Function is a very important pattern, we provide a special optimized Rule for it.

BaseExpressionRef function_pattern(
	const SymbolRef &head, const Definitions &definitions);

class FunctionRule : public Rule {
public:
	FunctionRule(const SymbolRef &head, const Definitions &definitions) :
		Rule(function_pattern(head, definitions)) {
	}

	virtual BaseExpressionRef try_apply(const Expression *args, const Evaluation &evaluation) const {
		const BaseExpressionRef &head = args->_head;
		if (head->type() != ExpressionType) {
			return BaseExpressionRef();
		}
		return head->as_expression()->with_slice<OptimizeForSpeed>([args, &evaluation] (const auto &head_slice) {

			switch (head_slice.size()) {
				case 1: { // Function[body_][args___]

					const BaseExpressionRef &body = head_slice[0];
					if (body->type() != ExpressionType) {
						return BaseExpressionRef();
					}
					const Expression *body_ptr = body->as_expression();
					return args->with_leaves_array(
						[body_ptr, &evaluation] (const BaseExpressionRef *slots, size_t n_slots) {
							return body_ptr->replace_slots(slots, n_slots, evaluation);
						});

				}

				case 2: { // Function[vars_, body_][args___]
					const BaseExpressionRef &vars = head_slice[0];
					if (vars->type() != ExpressionType) {
						return BaseExpressionRef();
					}
					const Expression *vars_ptr = vars->as_expression();

					if (vars_ptr->size() != args->size()) { // exit early if params don't match
						return BaseExpressionRef();
					}

					const BaseExpressionRef &body = head_slice[1];
					if (body->type() != ExpressionType) {
						return BaseExpressionRef();
					}
					const Expression *body_ptr = body->as_expression();

					return args->with_leaves_array(
						[vars_ptr, body_ptr, &evaluation]
						(const BaseExpressionRef *args, size_t n_args) {

							return vars_ptr->with_slice(
								[args, n_args, vars_ptr, body_ptr, &evaluation]
										(const auto &vars_slice) {

									const size_t n_vars = vars_slice.size();

									for (size_t i = 0; i < n_vars; i++) {
										if (vars_slice[i]->type() != SymbolType) {
											return BaseExpressionRef();
										}
									}

									BaseExpressionRef return_expr;

									for (size_t i = 0; i < n_vars; i++) {
										static_cast<const Symbol*>(vars_slice[i].get())->set_replacement(&args[i]);
									}

									try {
										return_expr = body_ptr->replace_vars(vars_ptr->cache()->name());
									} catch(...) {
										for (size_t i = 0; i < n_vars; i++) {
											static_cast<const Symbol*>(vars_slice[i].get())->clear_replacement();
										}
										throw;
									}

									for (size_t i = 0; i < n_vars; i++) {
										static_cast<const Symbol*>(vars_slice[i].get())->clear_replacement();
									}

									return return_expr;
								});
						}
					);
				}

				default:
					return BaseExpressionRef();
			}
		});
	}

	virtual MatchSize match_size() const {
		return MatchSize::at_least(0);
	}
};

void Builtins::Functional::initialize() {

	add("Function",
		Attributes::HoldAll, {
			NewRule<FunctionRule>
		}
	);
}
