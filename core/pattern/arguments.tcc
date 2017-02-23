#pragma once

inline SlotDirective CompiledArguments::operator()(const BaseExpressionRef &item) const {
    if (item->is_symbol()) {
        const index_t index = m_variables.find(
                static_cast<const Symbol*>(item.get()));
        if (index >= 0) {
            return SlotDirective::slot(index);
        } else {
            return SlotDirective::copy();
        }
    } else {
        if (item->is_expression()) {
            const Expression * const expr = item->as_expression();

            if (expr->head()->symbol() == S::OptionValue && expr->size() == 1) {
                const BaseExpressionRef &leaf = expr->n_leaves<1>()[0];
                if (leaf->is_symbol()) {
                    return SlotDirective::option_value(leaf->as_symbol());
                }
            }
        }

        return SlotDirective::descend();
    }
}
