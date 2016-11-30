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
		const Expression *head_ptr = static_cast<const Expression*>(head.get());

		switch (head_ptr->slice_code()) {
			case SliceCode::StaticSlice1Code: { // Function[body_][args___]
				const BaseExpressionRef &body = static_cast<const StaticSlice<1>*>(head_ptr->_slice_ptr)->leaf(0);
				if (body->type() != ExpressionType) {
					break;
				}
				const Expression *body_ptr = static_cast<const Expression*>(body.get());

				return args->with_leaves_array(
					[body_ptr, &evaluation] (const BaseExpressionRef *slots, size_t n_slots) {
						return body_ptr->replace_slots(slots, n_slots, evaluation);
					});
			}
			case SliceCode::StaticSlice2Code: { // Function[vars_, body_][args___]
				const auto *slice = static_cast<const StaticSlice<1>*>(head_ptr->_slice_ptr);

				const BaseExpressionRef &vars = slice->leaf(0);
				if (vars->type() != ExpressionType) {
					break;
				}
				const Expression *vars_ptr = static_cast<const Expression*>(vars.get());

				const BaseExpressionRef &body = slice->leaf(1);
				if (body->type() != ExpressionType) {
					break;
				}
				const Expression *body_ptr = static_cast<const Expression*>(body.get());

				return args->with_leaves_array(
					[vars_ptr, body_ptr, &evaluation] (const BaseExpressionRef *args, size_t n_args) {

						return vars_ptr->with_leaves_array(
							[args, n_args, vars_ptr, body_ptr, &evaluation]
							(const BaseExpressionRef *vars, size_t n_vars) {

								if (n_vars != n_args) {
									return BaseExpressionRef();
								}

								for (size_t i = 0; i < n_vars; i++) {
									if (vars[i]->type() != SymbolType) {
										return BaseExpressionRef();
									}
								}

								BaseExpressionRef return_expr;

								for (size_t i = 0; i < n_vars; i++) {
									static_cast<const Symbol*>(vars[i].get())->set_replacement(&args[i]);
								}

								try {
									return_expr = body_ptr->replace_vars(vars_ptr->cache()->name());
								} catch(...) {
									for (size_t i = 0; i < n_vars; i++) {
										static_cast<const Symbol*>(vars[i].get())->clear_replacement();
									}
									throw;
								}

								for (size_t i = 0; i < n_vars; i++) {
									static_cast<const Symbol*>(vars[i].get())->clear_replacement();
								}

								return return_expr;
							});
					});
			}

			default:
				// nothing
				break;
		}

		return BaseExpressionRef();
	}

	virtual MatchSize match_size() const {
		return MatchSize::at_least(0);
	}
};

void Builtin::Functional::initialize() {

	add("Function",
		Attributes::HoldAll, {
			NewRule<FunctionRule>
		}
	);
}
