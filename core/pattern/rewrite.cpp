#include "../runtime.h"

ExpressionRef RewriteExpression::rewrite_functions(
	const ExpressionPtr expr,
	const RewriteMask mask,
	const Evaluation &evaluation) {

	if (expr->head()->symbol() == S::Function && (mask & SlotRewriteMask) && expr->size() >= 2) {
		return expr->with_slice([expr, &evaluation] (const auto &slice) -> ExpressionRef {
			TemporaryRefVector names;
			ArgumentsMap renames;

			const auto add = [&names, &renames, &evaluation] (SymbolPtr arg) {
				std::string name(arg->as_symbol()->name());
				name.append("$");
				const BaseExpressionRef new_arg = evaluation.definitions.lookup(name.c_str());
				renames[arg->as_symbol()] = BaseExpressionRef(new_arg);
				names.push_back(new_arg);
			};

			const BaseExpressionRef args = slice[0];
			if (args->is_expression() && args->as_expression()->head()->symbol() == S::List) {
				args->as_expression()->with_slice([&names, &renames, &add] (const auto &args) {
					for (auto arg : args) {
						if (arg->is_symbol()) {
							add(arg->as_symbol());
						} else {
							names.push_back(BaseExpressionRef(arg));
						}
					}
				});
			} else if (args->is_symbol()) {
				add(args->as_symbol());
			} else {
				return expr;
			}

			const BaseExpressionRef new_args = names.to_expression(evaluation.List);

			const BaseExpressionRef new_body =
				coalesce(BaseExpressionRef(slice[1]->replace_all(renames, evaluation)), slice[1]);

			return expression(evaluation.Function, sequential([&new_args, &new_body, &slice] (auto &store) {
				store(BaseExpressionRef(new_args));
				store(BaseExpressionRef(new_body));
				for (size_t i = 2; i < slice.size(); i++) {
					store(BaseExpressionRef(slice[i]));
				}
			}, slice.size()));
		});
	} else {
		return ExpressionRef();
	}
}
