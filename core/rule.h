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

	virtual BaseExpressionRef try_apply(const Expression *expr, const Evaluation &evaluation) const = 0;

	virtual BaseExpressionRef rhs() const {
		throw std::runtime_error("no fixed right hand side is available for this Rule type");
	}

	virtual MatchSize match_size() const;

	inline optional<hash_t> match_hash() const {
		return pattern->match_hash();
	}
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

	virtual MatchSize match_size() const {
		return MatchSize::at_least(N);
	}
};

typedef std::shared_ptr<Rule> RuleRef;

class Rules {
public:
	struct Entry {
		RuleRef rule;
		optional<hash_t> match_hash;
	};

private:
	std::vector<Entry> m_rules[NumberOfSliceCodes];

public:
	void add(const RuleRef &rule);

	inline BaseExpressionRef operator()(
		SliceCode code,
		const Expression *expr,
		const Evaluation &evaluation) const;
};

#endif //CMATHICS_RULE_H
