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

class Rule : public AbstractHeapObject<Rule> {
public:
	const BaseExpressionRef pattern;
	SortKey key;

	inline Rule(const BaseExpressionRef &patt, const Evaluation &evaluation) : pattern(patt) {
		patt->pattern_key(key, evaluation);
	}

    virtual ~Rule() {
    }

	virtual optional<BaseExpressionRef> try_apply(
        const Expression *expr,
        const Evaluation &evaluation) const = 0;

	virtual BaseExpressionRef rhs() const {
		throw std::runtime_error("no fixed right hand side is available for this Rule type");
	}

	virtual MatchSize leaf_match_size() const;

	inline optional<hash_t> match_hash() const {
		return pattern->match_hash();
	}
};

BaseExpressionRef exactly_n_pattern(
	const SymbolRef &head, size_t n, const Evaluation &evaluation);

BaseExpressionRef at_least_n_pattern(
	const SymbolRef &head, size_t n, const Evaluation &evaluation);

template<size_t N>
class ExactlyNRule : public Rule {
public:
	ExactlyNRule(const SymbolRef &head, const Evaluation &evaluation) :
		Rule(exactly_n_pattern(head, N, evaluation), evaluation) {
	}

	virtual MatchSize match_size() const {
		return MatchSize::exactly(N);
	}
};

template<size_t N>
class AtLeastNRule : public Rule {
public:
	AtLeastNRule(const SymbolRef &head, const Evaluation &evaluation) :
		Rule(at_least_n_pattern(head, N, evaluation), evaluation) {
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
inline optional<BaseExpressionRef> RulesVector<Entry>::apply(
	const Expression *expr,
	Filter &filter,
	const Evaluation &evaluation) const {

	const auto slice_code = expr->slice_code();

	if (is_tiny_slice(slice_code)) {
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
inline optional<BaseExpressionRef> RulesVector<Entry>::apply(
	const std::vector<Entry> &entries,
	const Expression *expr,
	Filter &filter,
	const Evaluation &evaluation) const {

	for (const Entry &entry : entries) {
		if (!filter(entry)) {
			continue;
		}

		const optional<BaseExpressionRef> result =
			entry.try_apply(expr, evaluation);
		if (result) {
			return result;
		}
	}

	return optional<BaseExpressionRef>();
}

template<typename Entry>
bool RulesVector<Entry>::has_rule_with_pattern(
	std::vector<Entry> &entries,
	const BaseExpressionRef &pattern,
	const Evaluation &evaluation) {

	SortKey key;
	pattern->pattern_key(key, evaluation);

	const auto i = std::lower_bound(
		entries.begin(),
		entries.end(),
		key,
		[&evaluation] (const Entry &entry, const SortKey &key) {
			return entry.key().compare(key, evaluation) < 0;
		});

	return i != entries.end() && i->pattern()->same(pattern);
}

template<typename Entry>
void RulesVector<Entry>::insert_rule(
	std::vector<Entry> &entries,
	const Entry &entry,
	const Evaluation &evaluation) {

	const SortKey &key = entry.key();

	const auto i = std::lower_bound(
		entries.begin(),
		entries.end(),
		key,
		[&evaluation] (const Entry &entry, const SortKey &key) {
			return entry.key().compare(key, evaluation) < 0;
		});

	if (i != entries.end() && i->pattern()->same(entry.pattern())) {
		Entry::merge(entries, i, entry);
	} else {
		entries.insert(i, entry);
	}
}

template<typename Entry>
void RulesVector<Entry>::set_governing_attributes(
    Attributes attributes,
    const Evaluation &evaluation) {

    const bool new_is_match_size_known =
        is_match_size_known(attributes);

    if (new_is_match_size_known == m_is_match_size_known) {
        return;
    }

    m_is_match_size_known = new_is_match_size_known;

    for (size_t code = 0; code < NumberOfSliceCodes; code++) {
        m_rules[code].clear();
        for (const Entry &entry : m_all_rules) {
            if (!m_is_match_size_known || entry.size.matches(SliceCode(code))) {
                insert_rule(m_rules[code], entry, evaluation);
            }
        }
    }
}


template<typename Entry>
void RulesVector<Entry>::add(
	const typename Entry::RuleRef &rule,
	const Evaluation &evaluation) {

	const Entry entry(rule);

	for (size_t code = 0; code < NumberOfSliceCodes; code++) {
		if (!m_is_match_size_known || entry.size.matches(SliceCode(code))) {
			insert_rule(m_rules[code], entry, evaluation);
		}
	}

	insert_rule(m_all_rules, entry, evaluation);
}

template<typename Entry>
bool RulesVector<Entry>::has_rule_with_pattern(
	const BaseExpressionRef &lhs,
	const Evaluation &evaluation) {

	return has_rule_with_pattern(m_all_rules, lhs, evaluation);
}

inline RuleEntry::RuleEntry(const RuleRef &rule) :
	m_rule(rule),
	RuleHash(rule->leaf_match_size(), rule->match_hash()) {
}

inline optional<BaseExpressionRef> RuleEntry::try_apply(
	const Expression *expr,
    const Evaluation &evaluation) const {

	return m_rule->try_apply(expr, evaluation);
}

inline const auto &RuleEntry::key() const {
	return m_rule->key;
}

inline const auto &RuleEntry::pattern() const {
	return m_rule->pattern;
}

template<typename Expression>
inline optional<BaseExpressionRef> Rules::apply(
	const Expression *expr,
	const Evaluation &evaluation) const {

	NoRulesVectorFilter filter;
	return RulesVector<RuleEntry>::apply(expr, filter, evaluation);
}

inline FormatRuleRef FormatRule::merge(FormatRule *rule) {
    assert(m_rule->pattern->same(rule->m_rule->pattern));
    Forms forms(m_forms);
    for (const SymbolRef &symbol : rule->m_forms) {
        forms.insert(symbol);
    }
    return FormatRuleRef(FormatRule::construct(m_rule, forms));
}

inline FormatRuleEntry::FormatRuleEntry(
	const FormatRuleRef &rule) :

	m_rule(rule),
	RuleHash(rule->rule()->leaf_match_size(), rule->rule()->match_hash()) {
}

inline optional<BaseExpressionRef> FormatRuleEntry::try_apply(
	const Expression *expr,
    const Evaluation &evaluation) const {

	return m_rule->rule()->try_apply(expr, evaluation);
}

inline const auto &FormatRuleEntry::key() const {
	return m_rule->rule()->key;
}

inline const auto &FormatRuleEntry::pattern() const {
	return m_rule->rule()->pattern;
}

template<typename Entries, typename Iterator>
void FormatRuleEntry::merge(Entries &entries, const Iterator &i, const FormatRuleEntry &entry) {
	auto j = entries.insert(i, entry);
	j++;

	const BaseExpressionRef pattern = i->pattern();

	if (entry.m_rule->all_forms()) {
		// new rule replaces all existing other rules
		while (j != entries.end()) {
			if (!pattern->same(j->pattern())) {
				break;
			}
			j = entries.erase(j);
		}
	} else {
		// new rule erases forms from existing rules
		while (j != entries.end()) {
			if (!pattern->same(j->pattern())) {
				break;
			}
			if (j->m_rule->remove_forms(entry.m_rule->forms())) {
				j = entries.erase(j);
			} else {
				j++;
			}
		}

	}
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
inline optional<BaseExpressionRef> FormatRules::apply(
	const Expression *expr,
	const SymbolRef &form,
	const Evaluation &evaluation) const {

	FormFilter filter(form);
	return RulesVector<FormatRuleEntry>::apply(expr, filter, evaluation);
}

#endif //CMATHICS_RULE_H
