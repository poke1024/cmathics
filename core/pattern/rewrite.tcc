#pragma once

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
    m_leaves(nodes(arguments, body)) {
}

template<typename Arguments, typename Options>
inline BaseExpressionRef RewriteExpression::rewrite_or_copy(
    const Expression *body,
    const Arguments &args,
    const Options &options) const {

    return body->with_slice_c(
        [this, body, &args, &options] (const auto &slice) {
            const auto generate = [this, &slice, &args, &options] (auto &store) {
                const size_t n = slice.size();
                for (size_t i = 0; i < n; i++) {
                    store(m_leaves[i].rewrite_or_copy(slice[i], args, options));
                }
            };

            return ExpressionRef(expression(
                m_head.rewrite_or_copy(body->head(), args, options),
                sequential(generate, slice.size())))->flatten_sequence_or_copy();
        });
}
