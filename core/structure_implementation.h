#ifndef CMATHICS_STRUCTURE_IMPLEMENTATION_H
#define CMATHICS_STRUCTURE_IMPLEMENTATION_H

#include "structure.h"
#include "expression.h"

template<typename T>
BaseExpressionRef StructureOperationsImplementation<T>::replace_slots(
	const BaseExpressionRef *slots, size_t n_slots, const Evaluation &evaluation) const {

	const T &self = this->expr();
	const BaseExpressionRef &head = self._head;
	const auto &leaves = self._leaves;

	if (head->type() == SymbolType) {
		const Symbol *symbol = static_cast<const Symbol*>(head.get());
		switch(symbol->_id) {
			case SymbolId::Slot:
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

			case SymbolId::Function:
				if (leaves.size() == 1) {
					// do not replace Slots in nested Functions
					return BaseExpressionRef();
				}

			default: {
				// nothing
			}
		}
	}

	BaseExpressionRef new_head;
	if (head->type() == ExpressionType) {
		new_head = static_cast<const Expression*>(head.get())->replace_slots(slots, n_slots, evaluation);
	} else {
		new_head = head;
	}

	return self.apply(
		new_head,
        0,
		leaves.size(),
        [&slots, n_slots, &evaluation] (const BaseExpressionRef &ref) {
			return static_cast<const Expression*>(ref.get())->replace_slots(slots, n_slots, evaluation);
        },
		(1 << ExpressionType));
}

#endif //CMATHICS_STRUCTURE_IMPLEMENTATION_H
