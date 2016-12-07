#ifndef CMATHICS_STRUCTURE_IMPLEMENTATION_H
#define CMATHICS_STRUCTURE_IMPLEMENTATION_H

#include "structure.h"
#include "expression.h"
#include "evaluate.h"

template<typename T>
BaseExpressionRef StructureOperationsImplementation<T>::replace_vars(Name name) const {

	const T &self = this->expr();

	if (self.has_cache() && self.cache()->skip_replace_vars.contains(name)) {
		return BaseExpressionRef();
	}

	const BaseExpressionRef &head = self._head;
	const auto &leaves = self._leaves;

	auto replace = [name] (const BaseExpressionRef &expr) {
		const Type type = expr->type();

		if (type == SymbolType) {
			const BaseExpressionRef * const r =
				static_cast<const Symbol*>(expr.get())->replacement();
			if (r) {
				return *r;
			}
		} else if (type == ExpressionType) {
			return expr->as_expression()->replace_vars(name);
		}

		return BaseExpressionRef();
	};

	const BaseExpressionRef new_head = replace(head);

	const ExpressionRef result = transform(
		new_head ? new_head : head,
		leaves,
		0,
		leaves.size(),
		replace,
		(bool)new_head,
		MakeTypeMask(ExpressionType) | MakeTypeMask(SymbolType));

	if (!result) {
		self.cache()->skip_replace_vars.add(name);
	}

	return result;
}

#endif //CMATHICS_STRUCTURE_IMPLEMENTATION_H
