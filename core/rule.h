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

class Rule : public Shared<Rule, SharedHeap> {
public:
	const BaseExpressionRef pattern;
	const PatternSortKey key;

	Rule(const BaseExpressionRef &patt) : pattern(patt), key(patt->pattern_key()) {
	}

    virtual ~Rule() {
    }

	virtual BaseExpressionRef try_apply(const Expression *expr, const Evaluation &evaluation) const = 0;

	virtual BaseExpressionRef rhs() const {
		throw std::runtime_error("no fixed right hand side is available for this Rule type");
	}

	virtual MatchSize leaf_match_size() const;

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

typedef ConstSharedPtr<Rule> RuleRef;
typedef QuasiConstSharedPtr<Rule> CachedRuleRef;
typedef UnsafeSharedPtr<Rule> UnsafeRuleRef;

class Rules {
public:
	struct Entry {
		UnsafeRuleRef rule;
		MatchSize match_size;
		optional<hash_t> match_hash;
	};

private:
	std::vector<Entry> m_rules[NumberOfSliceCodes];

	template<bool CheckMatchSize>
	inline BaseExpressionRef do_try_and_apply(
		const std::vector<Entry> &entries,
		const Expression *expr,
		const Evaluation &evaluation) const;

public:
	void add(const RuleRef &rule);

	inline BaseExpressionRef try_and_apply(
		const SliceCode slice_code,
		const Expression *expr,
		const Evaluation &evaluation) const {

		if (is_static_slice(slice_code)) {
			return do_try_and_apply<false>(m_rules[slice_code], expr, evaluation);
		} else {
			return do_try_and_apply<true>(m_rules[slice_code], expr, evaluation);
		}
	}

	template<typename Slice>
	inline BaseExpressionRef try_and_apply(
		const Expression *expr,
		const Evaluation &evaluation) const {

		constexpr SliceCode slice_code = Slice::code();

		if (is_static_slice(slice_code)) {
			return do_try_and_apply<false>(m_rules[slice_code], expr, evaluation);
		} else {
			return do_try_and_apply<true>(m_rules[slice_code], expr, evaluation);
		}
	}
};

#endif //CMATHICS_RULE_H
