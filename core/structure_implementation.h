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
			storage << expression(f, StaticSlice<1>(&leaf, leaf->type_mask()));
		}
	}, leaves.size());
}

template<typename T>
BaseExpressionRef StructureOperationsImplementation<T>::replace_slots(
	const BaseExpressionRef *slots, size_t n_slots, const Evaluation &evaluation) const {

	const T &self = this->expr();
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
		new_head = static_cast<const Expression*>(head.get())->replace_slots(slots, n_slots, evaluation);
	}

	return apply(
		new_head ? new_head : head,
		leaves,
        0,
		leaves.size(),
        [&slots, n_slots, &evaluation] (const BaseExpressionRef &ref) {
			return static_cast<const Expression*>(ref.get())->replace_slots(slots, n_slots, evaluation);
        },
		(bool)new_head,
		MakeTypeMask(ExpressionType));
}

#endif //CMATHICS_STRUCTURE_IMPLEMENTATION_H
