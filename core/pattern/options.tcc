#pragma once

template<typename Assign>
bool parse_options(
    const Assign &assign,
    const BaseExpressionRef &item,
    const Evaluation &evaluation) {

    if (!item->is_expression()) {
        return false;
    }

    const Expression * const expr = item->as_expression();

    switch (expr->head()->symbol()) {
        case S::List:
            return expr->with_slice([&assign, &evaluation] (const auto &slice) {
                const size_t n = slice.size();
                for (size_t i = 0; i < n; i++) {
                    if (!parse_options(assign, slice[i], evaluation)) {
                        return false;
                    }
                }
                return true;
            });

        case S::Rule:
        case S::RuleDelayed:
            if (expr->size() == 2) {
                const auto * const leaves = expr->n_leaves<2>();
                UnsafeSymbolRef name;

                const auto &lhs = leaves[0];
                if (lhs->is_symbol()) {
                    name = lhs->as_symbol();
                } else if (lhs->is_string()) {
                    name = lhs->as_string()->option_symbol(evaluation);
                    if (!name) {
                        return false;
                    }
                } else {
                    return false;
                }

                const auto &rhs = leaves[1];
                assign(name.get(), rhs);

                return true;
            }
            return false;

        default:
            return false;
    }
}

template<typename Sequence, typename Assign, typename Rollback>
index_t OptionsProcessor::parse(
    const Sequence &sequence,
    index_t begin,
    index_t end,
    const Assign &assign,
    const Rollback &rollback,
    const MatchRest &rest) {

    index_t t = begin;
    while (t < end) {
        if (!parse_options(
            assign,
            *sequence.element(t),
            sequence.context().evaluation)) {

            break;
        }
        t++;
    }

    const index_t m = rest(begin, t, end);

    if (t > begin && m < 0) {
        rollback();
    }

    return m;
}

template<typename Sequence>
inline index_t DynamicOptionsProcessor::do_match(
    const Sequence &sequence,
    index_t begin,
    index_t end,
    const MatchRest &rest) {

    OptionsMap options(m_options);

    std::swap(options, m_options);

    return parse(sequence, begin, end,
         [this] (SymbolPtr name, const BaseExpressionRef &value) {
             m_options[name] = value;
         },
         [this, &options] () {
             std::swap(options, m_options);
         },
         rest);
}

template<typename Options>
index_t StaticOptionsProcessor<Options>::match(
    const SlowLeafSequence &sequence,
    index_t begin,
    index_t end,
    const MatchRest &rest) {

    return do_match(sequence, begin, end, rest);
}

template<typename Options>
index_t StaticOptionsProcessor<Options>::match(
    const FastLeafSequence &sequence,
    index_t begin,
    index_t end,
    const MatchRest &rest) {

    return do_match(sequence, begin, end, rest);
}

template<typename Options>
template<typename Sequence>
inline index_t StaticOptionsProcessor<Options>::do_match(
    const Sequence &sequence,
    index_t begin,
    index_t end,
    const MatchRest &rest) {

    optional<Options> saved_options;

    if (m_options_ptr == &m_options) {
        saved_options = m_options;
    }

    const auto &evaluation = sequence.context().evaluation;

    return parse(sequence, begin, end,
         [this, &evaluation] (SymbolPtr name, const BaseExpressionRef &value) {
             if (m_options_ptr != &m_options) {
                 m_options = m_controller.defaults();
                 m_options_ptr = &m_options;
             }

             m_controller.set(m_options, name, value, evaluation);
         },
         [this, &saved_options] () {
             if (saved_options) {
                 m_options = *saved_options;
             } else {
                 m_options_ptr = &m_controller.defaults();
             }
         }, rest);
}
