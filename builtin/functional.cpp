#include "functional.h"

// as Function is a very important pattern, we provide a special optimized Rule for it.

BaseExpressionRef function_pattern(
	const SymbolRef &head, const Definitions &definitions);

template<typename Slots>
inline BaseExpressionRef replace_slots(
	const Expression *expr,
	const Slots &slots,
	const Evaluation &evaluation) {

	if (expr->has_cache() && expr->cache()->skip_slots) {
		return BaseExpressionRef();
	}

	return expr->with_slice<CompileToSliceType>(
		[expr, &slots, &evaluation] (const auto &slice) -> BaseExpressionRef {
			const BaseExpressionRef &head = expr->head();

			switch (head->extended_type()) {
				case SymbolSlot:
					if (slice.size() != 1) {
						// error
					} else {
						const auto slot = slice[0];
						if (slot->type() == MachineIntegerType) {
							const machine_integer_t slot_id =
								static_cast<const MachineInteger*>(slot.get())->value;
							if (slot_id < 1) {
								// error
							} else if (slot_id > slots.size()) {
								// error
							} else {
								return slots[slot_id - 1];
							}
						} else {
							// error
						}
					}
					break;

				case SymbolFunction:
					if (slice.size() == 1) {
						// do not replace Slots in nested Functions
						return BaseExpressionRef();
					}

				default: {
					// nothing
				}
			}

			BaseExpressionRef new_head;
			if (head->type() == ExpressionType) {
				new_head = replace_slots(head->as_expression(), slots, evaluation);
			}

			const BaseExpressionRef result = apply(
				new_head ? new_head : head,
				slice,
				0,
				slice.size(),
				[&slots, &evaluation] (const BaseExpressionRef &ref) {
					return replace_slots(ref->as_expression(), slots, evaluation);
				},
				(bool)new_head,
				MakeTypeMask(ExpressionType));

			if (!result) {
				expr->cache()->skip_slots = true;
			}

			return result;
		});
}


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
		return head->as_expression()->with_slice<CompileToSliceType>([args, &evaluation] (const auto &head_slice) {

			switch (head_slice.size()) {
				case 1: { // Function[body_][args___]

					const BaseExpressionRef &body = head_slice[0];
					if (body->type() != ExpressionType) {
						return BaseExpressionRef();
					}
					return args->with_slice([&body, &evaluation] (const auto &slots) {
						return replace_slots(body->as_expression(), slots, evaluation);
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
								[args, n_args, vars_ptr, body_ptr, &evaluation] (const auto &vars_slice) {

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
