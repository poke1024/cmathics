#include "types.h"
#include "core/expression/implementation.h"
#include "rule.h"

BaseExpressionRef exactly_n_pattern(
    const SymbolRef &head, size_t n, const Evaluation &evaluation) {

    const auto &Blank = evaluation.Blank;
    return expression(head, sequential([n, &Blank] (auto &store) {
        for (size_t i = 0; i < n; i++) {
            store(expression(Blank));
        }
    }, n));
}

BaseExpressionRef at_least_n_pattern(
    const SymbolRef &head, size_t n, const Evaluation &evaluation) {

    const auto &Blank = evaluation.Blank;
    const auto &BlankNullSequence = evaluation.BlankNullSequence;

    return expression(head, sequential([n, &Blank, &BlankNullSequence] (auto &store) {
        for (size_t i = 0; i < n; i++) {
            store(expression(Blank));
        }
        store(expression(BlankNullSequence));
    }, n + 1));
}

BaseExpressionRef function_pattern(
    const SymbolRef &head, const Evaluation &evaluation) {

    const auto &BlankSequence = evaluation.BlankSequence;
    const auto &BlankNullSequence = evaluation.BlankNullSequence;

    return expression(expression(head, expression(BlankSequence)), expression(BlankNullSequence));
}

MatchSize Rule::leaf_match_size() const {
    if (!pattern->is_expression()) {
        return MatchSize::exactly(0); // undefined
    } else {
        return pattern->as_expression()->leaf_match_size();
    }
}
