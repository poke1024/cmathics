#pragma once

template<typename Arguments, typename Options>
inline BaseExpressionRef RewriteBaseExpression::rewrite_or_copy(
    const BaseExpressionRef &expr,
    const Arguments &args,
    const Options &options,
    const Evaluation &evaluation) const {

    const index_t slot = m_slot;

    if (slot >= 0) {
        return args(slot, expr);
    } else switch (slot) {
        case Copy:
            return expr;

        case Descend:
            return m_down->rewrite_or_copy(
                expr->as_expression(), args, options, evaluation);

        case OptionValue: {
            const auto option = options(m_option_value);
            if (option) {
                return option;
            }
            return expr;
        }

        case IllegalSlot: {
            evaluation.message(evaluation.Function, "slot", m_illegal_slot);
            return expr;
        }

        default:
            assert(false);
            return expr;
    }
};

template<typename Arguments>
std::vector<const RewriteBaseExpression> RewriteExpression::nodes(
    Arguments &arguments,
    const Expression *body) {

    return body->with_slice([&arguments] (const auto &slice) {
        std::vector<const RewriteBaseExpression> refs;
        const size_t n = slice.size();
        refs.reserve(n);
        for (size_t i = 0; i < n; i++) {
            refs.emplace_back(RewriteBaseExpression::construct(arguments, slice[i]));
        }
        return refs;
    });
}

template<typename Arguments>
RewriteExpression::RewriteExpression(
    Arguments &arguments,
    const Expression *body) :

    m_head(RewriteBaseExpression::construct(arguments, body->head())),
    m_leaves(nodes(arguments, body)),
    m_mask(compute_mask(m_head, m_leaves)) {
}

template<typename Arguments, typename Options>
inline BaseExpressionRef RewriteExpression::rewrite_or_copy(
    const Expression *body,
    const Arguments &args,
    const Options &options,
    const Evaluation &evaluation) const {

	if (body->head()->symbol() == S::Function && (mask() & SlotRewriteMask)) {
		// FIXME. rename function arguments.
	}

    return body->with_slice_c(
        [this, body, &args, &options, &evaluation] (const auto &slice) {
            const auto generate = [this, &slice, &args, &options, &evaluation] (auto &store) {
                const size_t n = slice.size();
                for (size_t i = 0; i < n; i++) {
                    store(m_leaves[i].rewrite_or_copy(slice[i], args, options, evaluation));
                }
            };

            return ExpressionRef(expression(
                m_head.rewrite_or_copy(body->head(), args, options, evaluation),
                sequential(generate, slice.size())))->flatten_sequence_or_copy();
        });
}
