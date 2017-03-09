#pragma once
// @formatter:off

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
                args, options, evaluation);

        case OptionValue: {
            const auto option = options(m_option_value);
            if (option) {
                return option;
            }
            return expr;
        }

        case IllegalSlot: {
            evaluation.message(
                evaluation.Function, "slot", m_illegal_slot);
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
    const Expression *expr,
    const Evaluation &evaluation) {

    return expr->with_slice([&arguments, &evaluation] (const auto &slice) {
        std::vector<const RewriteBaseExpression> refs;
        const size_t n = slice.size();
        refs.reserve(n);
        for (size_t i = 0; i < n; i++) {
            refs.emplace_back(RewriteBaseExpression::from_arguments(arguments, slice[i], evaluation));
        }
        return refs;
    });
}

template<typename Arguments>
RewriteExpressionRef RewriteExpression::from_arguments(
    Arguments &arguments,
    const Expression *expr,
    const Evaluation &evaluation,
    bool is_rewritten) {

    const RewriteBaseExpression head(
        RewriteBaseExpression::from_arguments(arguments, expr->head(), evaluation));
    std::vector<const RewriteBaseExpression> leaves(
        RewriteExpression::nodes(arguments, expr, evaluation));

    RewriteMask mask = head.mask();
    for (const RewriteBaseExpression &leaf : leaves) {
        mask |= leaf.mask();
    }

    if (!is_rewritten) {
        const ExpressionRef new_expr(rewrite_functions(expr, mask, evaluation));

        // if a rewrite of the expression happened, we actually also need to redo
        // all our RewriteBaseExpressions (since the embedded RewritesExpressions
        // in RewriteBaseExpression::m_down need updating too).

        if (new_expr) {
            return from_arguments(arguments, new_expr.get(), evaluation, true);
        }
    }

    return RewriteExpressionRef(
        RewriteExpression::construct(head, std::move(leaves), mask, expr));
}

template<typename Arguments, typename Options>
inline BaseExpressionRef RewriteExpression::rewrite_or_copy(
    const Arguments &args,
    const Options &options,
    const Evaluation &evaluation) const {

    return m_expr->with_slice_c(
        [this, &args, &options, &evaluation] (const auto &slice) {
            const auto generate = [this, &slice, &args, &options, &evaluation] (auto &store) {
                const size_t n = slice.size();
                for (size_t i = 0; i < n; i++) {
                    store(m_leaves[i].rewrite_or_copy(slice[i], args, options, evaluation));
                }
            };

            return ExpressionRef(expression(
                m_head.rewrite_or_copy(m_expr->head(), args, options, evaluation),
                sequential(generate, slice.size())))->flatten_sequence_or_copy();
        });
}
