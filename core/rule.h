#ifndef CMATHICS_RULE_H
#define CMATHICS_RULE_H

#include "types.h"

enum class DefinitionsPos : int {
	None,
	Own,
    Up,
	Down,
	Sub
};

#include "sort.h"

class Rule : public Shared<Rule, SharedHeap> {
public:
	const BaseExpressionRef pattern;
	const SortKey key;

	inline Rule(const BaseExpressionRef &patt) : pattern(patt), key(patt->pattern_key()) {
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

class NoRulesVectorFilter {
public:
	inline bool operator()(const RuleHash &data) const {
		return true;
	}
};

class RulesVectorFilter {
private:
	const Expression * const m_expr;
	hash_t m_hash;
	bool m_hash_valid;

	inline hash_t hash() {
		if (!m_hash_valid) {
			m_hash = m_expr->hash();
			m_hash_valid = true;
		}
		return m_hash;
	}

public:
	inline RulesVectorFilter(const Expression *expr) :
		m_expr(expr), m_hash_valid(false) {
	}

	inline bool operator()(const RuleHash &data) {
		if (data.hash) {
			if (hash() != *data.hash) {
				return false;
			}
		}

		return true;
	}
};

template<typename Filter>
class RulesVectorSizeFilter {
private:
	Filter &m_filter;
	const size_t m_size;

public:
	inline RulesVectorSizeFilter(const Expression *expr, Filter &filter) :
		m_filter(filter), m_size(expr->size()) {
	}

	template<typename Entry>
	inline bool operator()(const Entry &entry) {
		if (!entry.size.contains(m_size)) {
			return false;
		}

		return m_filter(entry);
	}
};

template<typename Entry>
template<typename Expression, typename Filter>
inline BaseExpressionRef RulesVector<Entry>::apply(
	const Expression *expr,
	Filter &filter,
	const Evaluation &evaluation) const {

	const auto slice_code = expr->slice_code();

	if (is_static_slice(slice_code)) {
		return apply(
			m_rules[slice_code], expr, filter, evaluation);
	} else {
		RulesVectorSizeFilter<Filter> filter2(expr, filter);
		return apply(
			m_rules[slice_code], expr, filter2, evaluation);
	}
}

template<typename Entry>
template<typename Filter>
inline BaseExpressionRef RulesVector<Entry>::apply(
	const std::vector<Entry> &entries,
	const Expression *expr,
	Filter &filter,
	const Evaluation &evaluation) const {

	for (const Entry &entry : entries) {
		if (!filter(entry)) {
			continue;
		}

		const BaseExpressionRef result =
			entry.try_apply(expr, evaluation);
		if (result) {
			return result;
		}
	}

	return BaseExpressionRef();
}

template<typename Entry>
void RulesVector<Entry>::insert_rule(std::vector<Entry> &entries, const Entry &entry) {
	static struct {
		inline bool operator()(const Entry &entry, const SortKey &key) const {
			return entry.key().compare(key) < 0;
		}
	} CompareSortKey;

	const SortKey &key = entry.key();
	const auto i = std::lower_bound(
		entries.begin(), entries.end(), key, CompareSortKey);
	if (i != entries.end() && i->pattern()->same(entry.pattern())) {
		i->merge(entry);
	} else {
		entries.insert(i, entry);
	}
}

template<typename Entry>
void RulesVector<Entry>::add(const typename Entry::RuleRef &rule) {
	const Entry entry(rule);

	for (size_t code = 0; code < NumberOfSliceCodes; code++) {
		if (entry.size.matches(SliceCode(code))) {
			insert_rule(m_rules[code], entry);
		}
	}
}

inline RuleEntry::RuleEntry(const RuleRef &rule) :
	m_rule(rule),
	RuleHash(rule->leaf_match_size(), rule->match_hash()) {
}

inline BaseExpressionRef RuleEntry::try_apply(
	const Expression *expr, const Evaluation &evaluation) const {

	return m_rule->try_apply(expr, evaluation);
}

inline const auto &RuleEntry::key() const {
	return m_rule->key;
}

inline const auto &RuleEntry::pattern() const {
	return m_rule->pattern;
}

template<typename Expression>
inline BaseExpressionRef Rules::apply(
	const Expression *expr,
	const Evaluation &evaluation) const {

	NoRulesVectorFilter filter;
	return RulesVector<RuleEntry>::apply(expr, filter, evaluation);
}

inline FormatRuleEntry::FormatRuleEntry(
	const FormatRuleRef &rule) :

	m_rule(rule),
	RuleHash(rule->rule()->leaf_match_size(), rule->rule()->match_hash()) {
}

inline BaseExpressionRef FormatRuleEntry::try_apply(
	const Expression *expr, const Evaluation &evaluation) const {

	return m_rule->rule()->try_apply(expr, evaluation);
}

inline const auto &FormatRuleEntry::key() const {
	return m_rule->rule()->key;
}

inline const auto &FormatRuleEntry::pattern() const {
	return m_rule->rule()->pattern;
}

class FormFilter {
private:
	const SymbolRef &m_form;

public:
	inline FormFilter(const SymbolRef &form) : m_form(form) {
	}

	inline bool operator()(const FormatRuleEntry &entry) const {
		return entry.has_form(m_form);
	}
};

template<typename Expression>
inline BaseExpressionRef FormatRules::apply(
	const Expression *expr,
	const SymbolRef &form,
	const Evaluation &evaluation) const {

	FormFilter filter(form);
	return RulesVector<FormatRuleEntry>::apply(expr, filter, evaluation);
}

#endif //CMATHICS_RULE_H
