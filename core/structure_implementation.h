#ifndef CMATHICS_STRUCTURE_IMPLEMENTATION_H
#define CMATHICS_STRUCTURE_IMPLEMENTATION_H

#include "structure.h"
#include "expression.h"
#include "evaluate.h"

template<typename T>
BaseExpressionRef StructureOperationsImplementation<T>::map(
	const BaseExpressionRef &f, const Evaluation &evaluation) const {

	const T &self = this->expr();
	const auto &leaves = self._leaves;

	return expression(self._head, [&f, &leaves] (auto &storage) {
		for (const BaseExpressionRef &leaf : leaves.leaves()) {
			storage << expression(f, StaticSlice<1>(&leaf, leaf->base_type_mask()));
		}
	}, leaves.size());
}

template<typename T>
BaseExpressionRef StructureOperationsImplementation<T>::select(
	const BaseExpressionRef &cond, const Evaluation &evaluation) const {

	const T &self = this->expr();
	const auto &leaves = self._leaves;
	const size_t size = self.size();

	std::vector<BaseExpressionRef> remaining;
	remaining.reserve(self.size());

	for (size_t i = 0; i < size; i++) {
		const auto leaf = leaves[i];
		if (expression(cond, leaf)->evaluate(evaluation)->is_true()) {
			remaining.push_back(leaf);
		}
	}

	return expression(self.head(), std::move(remaining));
}

template<typename T>
ExpressionRef StructureOperationsImplementation<T>::iterate(
	Symbol *iterator, const BaseExpressionRef &expr, const Evaluation &evaluation) const {

	const T &self = this->expr();

	return expression(
		evaluation.List,
	    [iterator, &self, &expr, &evaluation] (auto &storage) {
		    const auto &leaves = self._leaves.leaves();

		    for (const BaseExpressionRef &leaf : leaves) {
			    storage << scope(
				    iterator,
				    leaf,
				    [&expr, &evaluation] () {
					    return expr->evaluate(evaluation);
				    });
		    }
	    },
	    self._leaves.size()
	);
}

template<typename T>
BaseExpressionRef StructureOperationsImplementation<T>::replace_slots(
	const BaseExpressionRef *slots, size_t n_slots, const Evaluation &evaluation) const {

	const T &self = this->expr();

	if (self.has_cache() && self.cache()->skip_slots) {
		return BaseExpressionRef();
	}

	const BaseExpressionRef &head = self._head;
	const auto &leaves = self._leaves;

    switch (head->extended_type()) {
        case SymbolSlot:
            if (leaves.size() != 1) {
                // error
            } else {
                const auto slot = leaves[0];
                if (slot->type() == MachineIntegerType) {
                    const machine_integer_t slot_id =
                            static_cast<const MachineInteger*>(slot.get())->value;
                    if (slot_id < 1) {
                        // error
                    } else if (slot_id > n_slots) {
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
            if (leaves.size() == 1) {
                // do not replace Slots in nested Functions
                return BaseExpressionRef();
            }

        default: {
            // nothing
        }
    }

	BaseExpressionRef new_head;
	if (head->type() == ExpressionType) {
		new_head = head->as_expression()->replace_slots(slots, n_slots, evaluation);
	}

	const ExpressionRef result = apply(
		new_head ? new_head : head,
		leaves,
        0,
		leaves.size(),
        [&slots, n_slots, &evaluation] (const BaseExpressionRef &ref) {
			return ref->as_expression()->replace_slots(slots, n_slots, evaluation);
        },
		(bool)new_head,
		MakeTypeMask(ExpressionType));

	if (!result) {
		self.cache()->skip_slots = true;
	}

	return result;
}

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

	const ExpressionRef result = apply(
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
