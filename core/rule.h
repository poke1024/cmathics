#ifndef CMATHICS_RULE_H
#define CMATHICS_RULE_H

#include "types.h"

enum class DefinitionsPos : int {
	None,
	Down,
	Sub
};

class Rule {
public:
	virtual MatchSize match_size() const = 0;

	virtual BaseExpressionRef try_apply(const ExpressionRef &expr, const Evaluation &evaluation) const = 0;

	virtual DefinitionsPos get_definitions_pos(const SymbolRef &symbol) const = 0;
};

class QuickBuiltinRule : public Rule {
private:
	virtual DefinitionsPos get_definitions_pos(const SymbolRef &symbol) const {
		return DefinitionsPos::Down;
	}

	virtual MatchSize match_size() const {
		return MatchSize::at_least(0); // override if this is too broad
	}
};

typedef std::shared_ptr<Rule> RuleRef;

typedef std::vector<RuleRef> RuleTable[NumberOfSliceCodes];

#endif //CMATHICS_RULE_H
