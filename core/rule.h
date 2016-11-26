#ifndef CMATHICS_RULE_H
#define CMATHICS_RULE_H

#include "types.h"

enum class DefinitionsPos : int {
	None,
	Own,
	Down,
	Sub
};

#include "sort.h"

class Rule {
public:
	const BaseExpressionRef pattern;
	const SortKey key;

	Rule(const BaseExpressionRef &patt) : pattern(patt), key(patt->pattern_key()) {
	}

	virtual BaseExpressionRef try_apply(const ExpressionRef &expr, const Evaluation &evaluation) const = 0;

	virtual DefinitionsPos get_definitions_pos(const SymbolRef &symbol) const;

	virtual MatchSize match_size() const;
};

BaseExpressionRef exactly_n_pattern(
	const SymbolRef &head, size_t n, const Definitions &definitions);

BaseExpressionRef at_least_n_pattern(
	const SymbolRef &head, size_t n, const Definitions &definitions);

template<size_t N>
class ExactlyNRule : public Rule {
public:
	ExactlyNRule(const SymbolRef &head, const Definitions &definitions) :
		Rule(exactly_n_pattern(head, N, definitions)) {
	}

	virtual DefinitionsPos get_definitions_pos(const SymbolRef &symbol) const {
		return DefinitionsPos::Down;
	}

	virtual MatchSize match_size() const {
		return MatchSize::exactly(N);
	}
};

template<size_t N>
class AtLeastNRule : public Rule {
public:
	AtLeastNRule(const SymbolRef &head, const Definitions &definitions) :
		Rule(at_least_n_pattern(head, N, definitions)) {
	}

	virtual DefinitionsPos get_definitions_pos(const SymbolRef &symbol) const {
		return DefinitionsPos::Down;
	}

	virtual MatchSize match_size() const {
		return MatchSize::at_least(N);
	}
};

typedef std::shared_ptr<Rule> RuleRef;

typedef std::vector<RuleRef> RuleTable[NumberOfSliceCodes];

#endif //CMATHICS_RULE_H
