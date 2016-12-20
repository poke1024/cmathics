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

			const BaseExpressionRef result = transform<MakeTypeMask(ExpressionType)>(
				new_head ? new_head : head,
				slice,
				0,
				slice.size(),
				[&slots, &evaluation] (const BaseExpressionRef &ref) {
					return replace_slots(ref->as_expression(), slots, evaluation);
				},
				(bool)new_head);

			if (!result) {
				expr->cache()->skip_slots = true;
			}

			return result;
		});
}

inline BaseExpressionRef replace_vars(
	Name name,
	const VariableMap &variables,
	const Expression *body_ptr) {

	if (body_ptr->has_cache() && body_ptr->cache()->skip_replace_vars.contains(name)) {
		return BaseExpressionRef();
	}

	const BaseExpressionRef &head = body_ptr->head();

	auto replace = [name, &variables] (const BaseExpressionRef &expr) {
		const Type type = expr->type();

		if (type == SymbolType) {
			const auto i = variables.find(static_cast<const Symbol*>(expr.get()));
			if (i != variables.end()) {
				return i->second;
			}
		} else if (type == ExpressionType) {
			return replace_vars(name, variables, expr->as_expression());
		}

		return BaseExpressionRef();
	};

	const BaseExpressionRef new_head = replace(head);

	const ExpressionRef result = body_ptr->with_slice([&head, &new_head, &replace] (const auto &slice) {
		return transform<MakeTypeMask(ExpressionType) | MakeTypeMask(SymbolType)>(
			new_head ? new_head : head,
			slice,
			0,
			slice.size(),
			replace,
			(bool)new_head);
	});

	if (!result) {
		body_ptr->cache()->skip_replace_vars.add(name);
	}

	return result;
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

									VariableMap vars(Heap::variable_map());

									for (size_t i = 0; i < n_vars; i++) {
										vars[static_cast<const Symbol*>(vars_slice[i].get())] = args[i];
									}

									return replace_vars(vars_ptr->cache()->name(), vars, body_ptr);
								});
						}
					);
				}

				default:
					return BaseExpressionRef();
			}
		});
	}

	virtual MatchSize leaf_match_size() const {
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
