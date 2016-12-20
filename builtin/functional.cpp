#include "functional.h"

// as Function is a very important pattern, we provide a special optimized Rule for it.

BaseExpressionRef function_pattern(
	const SymbolRef &head, const Definitions &definitions);

template<typename Slots>
inline BaseExpressionRef replace_slots(
	const Expression *expr,
	const Slots &slots,
	const Evaluation &evaluation) {

	const CacheRef cache = expr->get_cache();

	if (cache && cache->skip_slots) {
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
				expr->ensure_cache()->skip_slots = true;
			}

			return result;
		});
}

inline BaseExpressionRef replace_vars(
	const ReplaceCacheRef &cache,
	std::unordered_set<const Expression *> &new_cache,
	const VariableMap<const BaseExpressionRef*> &variables,
	const Expression *body_ptr) {

	if (cache && cache->skip(body_ptr)) {
		return BaseExpressionRef();
	}

	const BaseExpressionRef &head = body_ptr->head();

	auto replace = [&cache, &new_cache, &variables] (const BaseExpressionRef &expr) {
		const Type type = expr->type();

		if (type == SymbolType) {
			const auto i = variables.find(static_cast<const Symbol*>(expr.get()));
			if (i != variables.end()) {
				return *(i->second);
			}
		} else if (type == ExpressionType) {
			return replace_vars(cache, new_cache, variables, expr->as_expression());
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
		new_cache.insert(body_ptr);
	}

	return result;
}

class FunctionRule : public Rule {
public:
	FunctionRule(const SymbolRef &head, const Definitions &definitions) :
		Rule(function_pattern(head, definitions)) {
	}

	virtual BaseExpressionRef try_apply(const Expression *function, const Evaluation &evaluation) const {
		const BaseExpressionRef &head = function->_head;
		if (head->type() != ExpressionType) {
			return BaseExpressionRef();
		}
		return head->as_expression()->with_slice<CompileToSliceType>([function, &evaluation] (const auto &head_slice) {

			switch (head_slice.size()) {
				case 1: { // Function[body_][args___]

					const BaseExpressionRef &body = head_slice[0];
					if (body->type() != ExpressionType) {
						return BaseExpressionRef();
					}
					return function->with_slice([&body, &evaluation] (const auto &slots) {
						return replace_slots(body->as_expression(), slots, evaluation);
					});
				}

				case 2: { // Function[vars_, body_][args___]
					const BaseExpressionRef &vars = head_slice[0];
					if (vars->type() != ExpressionType) {
						return BaseExpressionRef();
					}
					const Expression *vars_ptr = vars->as_expression();

					if (vars_ptr->size() != function->size()) { // exit early if params don't match
						return BaseExpressionRef();
					}

					const BaseExpressionRef &body = head_slice[1];
					if (body->type() != ExpressionType) {
						return BaseExpressionRef();
					}
					const Expression *body_ptr = body->as_expression();

					return function->with_leaves_array(
						[vars_ptr, body_ptr, &evaluation, &function]
						(const BaseExpressionRef *args, size_t n_args) {

							return vars_ptr->with_slice(
								[args, n_args, vars_ptr, body_ptr, &evaluation, &function] (const auto &vars_slice) {

									const size_t n_vars = vars_slice.size();

									for (size_t i = 0; i < n_vars; i++) {
										if (vars_slice[i]->type() != SymbolType) {
											return BaseExpressionRef();
										}
									}

									VariablePtrMap vars(Heap::variable_ptr_map_allocator());

									for (size_t i = 0; i < n_vars; i++) {
										vars[static_cast<const Symbol*>(vars_slice[i].get())] = &args[i];
									}

									const CacheRef cache = function->get_cache();
									ReplaceCacheRef replace_cache =
										cache ? cache->replace_cache : ReplaceCacheRef();
									std::unordered_set<const Expression*> new_cache;

									const BaseExpressionRef result = replace_vars(
										replace_cache, new_cache, vars, body_ptr);

									if (!cache && !new_cache.empty()) {
										function->ensure_cache()->replace_cache =
											std::make_shared<ReplaceCache>(std::move(new_cache));
									}

									return result;
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
