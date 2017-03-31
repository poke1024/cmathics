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

inline MatchSize match_size(const BaseExpressionRef &pattern) {
	if (!pattern->is_expression()) {
		return MatchSize::exactly(0); // undefined
	} else {
		if (pattern->as_expression()->head()->symbol() == S::Condition &&
		    pattern->as_expression()->size() == 2) {
			return match_size(pattern->as_expression()->n_leaves<2>()[0]);
		} else {
			return pattern->as_expression()->leaf_match_size();
		}
	}
}

MatchSize Rule::leaf_match_size() const {
	return match_size(pattern);
}
