#include "types.h"
#include "core/expression/implementation.h"
#include "rule.h"

BaseExpressionRef exactly_n_pattern(
    const SymbolRef &head, size_t n, const Definitions &definitions) {

    const auto &Blank = definitions.symbols().Blank;
    return expression(head, sequential([n, &Blank] (auto &store) {
        for (size_t i = 0; i < n; i++) {
            store(expression(Blank));
        }
    }, n));
}

BaseExpressionRef at_least_n_pattern(
    const SymbolRef &head, size_t n, const Definitions &definitions) {

    const auto &symbols = definitions.symbols();

    const auto &Blank = symbols.Blank;
    const auto &BlankNullSequence = symbols.BlankNullSequence;

    return expression(head, sequential([n, &Blank, &BlankNullSequence] (auto &store) {
        for (size_t i = 0; i < n; i++) {
            store(expression(Blank));
        }
        store(expression(BlankNullSequence));
    }, n + 1));
}

BaseExpressionRef function_pattern(
    const SymbolRef &head, const Definitions &definitions) {

    const auto &symbols = definitions.symbols();

    const auto &BlankSequence = symbols.BlankSequence;
    const auto &BlankNullSequence = symbols.BlankNullSequence;

    return expression(expression(head, expression(BlankSequence)), expression(BlankNullSequence));
}

MatchSize Rule::leaf_match_size() const {
    if (!pattern->is_expression()) {
        return MatchSize::exactly(0); // undefined
    } else {
        return pattern->as_expression()->leaf_match_size();
    }
}
