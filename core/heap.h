#ifndef CMATHICS_HEAP_H
#define CMATHICS_HEAP_H

#include <static_if/static_if.hpp>

#include <forward_list>
#include <unordered_set>

#include "gmpxx.h"
#include <arb.h>

#include "attributes.h"
#include "concurrent/pool.h"

class MachineInteger;
class BigInteger;

class MachineRational;
class BigRational;
class BigRational;

class Precision;

class MachineReal;
class BigReal;

class MachineComplex;
class BigComplex;

class RefsExtent;
typedef ConstSharedPtr<RefsExtent> RefsExtentRef;

class StringExtent;
typedef ConstSharedPtr<StringExtent> StringExtentRef;

template<int N>
class TinySlice;

class BigSlice;

template<typename U>
class PackedSlice;

template<typename Slice>
class ExpressionImplementation;

template<typename U>
using PackedExpression = ExpressionImplementation<PackedSlice<U>>;

template<typename U>
using PackedExpressionRef = ConstSharedPtr<ExpressionImplementation<PackedSlice<U>>>;

template<int N>
using TinyExpression = ExpressionImplementation<TinySlice<N>>;

template<int N>
using TinyExpressionRef = ConstSharedPtr<ExpressionImplementation<TinySlice<N>>>;

class Rule;

typedef ConstSharedPtr<Rule> RuleRef;
typedef QuasiConstSharedPtr<Rule> CachedRuleRef;
typedef UnsafeSharedPtr<Rule> UnsafeRuleRef;

struct RuleHash {
	MatchSize size;
	optional<hash_t> hash;

	inline RuleHash(
		const MatchSize &size_,
		const optional<hash_t> &hash_) :

		size(size_),
		hash(hash_) {
	}
};

class NoRulesVectorFilter;

template<typename Entry>
class RulesVector {
protected:
    std::vector<Entry> m_rules[NumberOfSliceCodes];
	std::vector<Entry> m_all_rules;
    bool m_is_match_size_known;

	template<typename Filter>
	inline optional<BaseExpressionRef> apply(
		const std::vector<Entry> &entries,
		const Expression *expr,
		Filter &filter,
		const Evaluation &evaluation) const;

	bool has_rule_with_pattern(
		std::vector<Entry> &entries,
		const BaseExpressionRef &lhs,
		const Evaluation &evaluation);

	void insert_rule(
		std::vector<Entry> &entries,
		const Entry &entry,
		const Evaluation &evaluation);

	template<typename Expression, typename Filter>
	inline optional<BaseExpressionRef> apply(
		const Expression *expr,
		Filter &filter,
		const Evaluation &evaluation) const;

public:
    RulesVector() : m_is_match_size_known(true) {
    }

    void set_governing_attributes(
        Attributes attributes,
        const Evaluation &evaluation);

    void add(
	    const typename Entry::RuleRef &rule,
	    const Evaluation &evaluation);

	bool has_rule_with_pattern(
		const BaseExpressionRef &lhs,
		const Evaluation &evaluation);

	inline auto begin() const {
		return m_all_rules.begin();
	}

	inline auto end() const {
		return m_all_rules.end();
	}
};

class RuleEntry : public RuleHash {
private:
	UnsafeRuleRef m_rule;

public:
	using RuleRef = ::RuleRef;

	inline RuleEntry(const RuleRef &rule);

	inline optional<BaseExpressionRef> try_apply(
		const Expression *expr, const Evaluation &evaluation) const;

	inline const auto &key() const;

	inline const auto &pattern() const;

	inline const auto &rule() const {
		return m_rule;
	}

    template<typename Entries, typename Iterator>
	static void merge(const Entries &entries, const Iterator &i, const RuleEntry &entry) {
		*i = entry;
	}
};

class Rules : public RulesVector<RuleEntry> {
public:
	using RulesVector<RuleEntry>::RulesVector;

	template<typename Expression>
	inline optional<BaseExpressionRef> apply(
		const Expression *expr,
		const Evaluation &evaluation) const;
};

class FormatRule;

typedef UnsafeSharedPtr<FormatRule> UnsafeFormatRuleRef;
typedef ConstSharedPtr<FormatRule> FormatRuleRef;

class FormatRule : public HeapObject<FormatRule> {
public:
	using Forms = std::unordered_set<SymbolRef, SymbolHash, SymbolEqual>;

private:
	const RuleRef m_rule;
    const bool m_all_forms;
	Forms m_forms;

public:
    FormatRule(const RuleRef &rule) :
        m_rule(rule), m_all_forms(true) {
    }

	FormatRule(const RuleRef &rule, const SymbolRef &form) :
		m_rule(rule), m_all_forms(false), m_forms(&form, &form + 1) {
	}

	FormatRule(const RuleRef &rule, const Forms &forms) :
		m_rule(rule), m_all_forms(false), m_forms(forms) {
	}

    const RuleRef &rule() const {
        return m_rule;
    }

	inline FormatRuleRef merge(FormatRule *rule);

    inline bool all_forms() const {
        return m_all_forms;
    }

	inline const Forms &forms() const {
		return m_forms;
	}

	inline bool has_form(const SymbolRef &form) const {
		return m_all_forms || m_forms.find(form) != m_forms.end();
	}

	inline bool remove_forms(const Forms &forms) {
		if (m_all_forms) {
			return false;
		} else {
			for (const auto &form : forms) {
				m_forms.erase(form);
			}
			return m_forms.empty();
		}
	}
};

class FormatRuleEntry : public RuleHash {
private:
	UnsafeFormatRuleRef m_rule;

public:
	using RuleRef = ::FormatRuleRef;

	inline FormatRuleEntry(
		const FormatRuleRef &rule);

	inline optional<BaseExpressionRef> try_apply(
		const Expression *expr, const Evaluation &evaluation) const;

	inline const auto &key() const;

	inline const auto &pattern() const;

    template<typename Entries, typename Iterator>
    static void merge(Entries &entries, const Iterator &i, const FormatRuleEntry &entry);

	inline bool has_form(const SymbolRef &form) const {
		return m_rule->has_form(form);
	}
};

class FormatRules : public RulesVector<FormatRuleEntry> {
public:
	using RulesVector<FormatRuleEntry>::RulesVector;

	template<typename Expression>
	inline optional<BaseExpressionRef> apply(
		const Expression *expr,
		const SymbolRef &form,
		const Evaluation &evaluation) const;
};

using SymbolRulesMap = CustomAllocatedMap<SymbolRefMap<Rules>>;

class Cache;

typedef ConstSharedPtr<Cache> CacheRef;
typedef QuasiConstSharedPtr<Cache> CachedCacheRef;
typedef UnsafeSharedPtr<Cache> UnsafeCacheRef;

#include "core/pattern/arguments.h"
#include "core/matcher/matcher.h"
#include "core/pattern/options.h"
#include "core/pattern/match.h"
#include "slice/heap.h"

class LegacyPool {
private:
	static LegacyPool *_s_instance;

private:
    SlotAllocator _slots;
    VectorAllocator<BaseExpressionRef> _ref_vector_allocator;

public:
	static inline auto &slots_allocator() {
		return _s_instance->_slots;
	}

    static inline auto &safe_ref_vector_allocator() {
        return _s_instance->_ref_vector_allocator;
    }

public:
    static void init();
};

#endif //CMATHICS_HEAP_H
