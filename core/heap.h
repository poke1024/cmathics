#ifndef CMATHICS_HEAP_H
#define CMATHICS_HEAP_H

#include <static_if/static_if.hpp>

#include <forward_list>
#include <unordered_set>

#include "gmpxx.h"
#include <arb.h>

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

	template<typename Filter>
	inline optional<BaseExpressionRef> apply(
		const std::vector<Entry> &entries,
		const Expression *expr,
		Filter &filter,
		const Evaluation &evaluation) const;

	void insert_rule(
		std::vector<Entry> &entries,
		const Entry &entry);

	template<typename Expression, typename Filter>
	inline optional<BaseExpressionRef> apply(
		const Expression *expr,
		Filter &filter,
		const Evaluation &evaluation) const;

public:
    void add(const typename Entry::RuleRef &rule);
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

class FormatRule : public Shared<FormatRule, SharedHeap> {
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

using SymbolRulesMap = SymbolRefMap<Rules>;

class Cache;

typedef ConstSharedPtr<Cache> CacheRef;
typedef QuasiConstSharedPtr<Cache> CachedCacheRef;
typedef UnsafeSharedPtr<Cache> UnsafeCacheRef;

#include "core/pattern/arguments.h"
#include "core/pattern/matcher.h"
#include "core/pattern/options.h"
#include "core/pattern/match.h"
#include "slice/heap.h"

class Pool {
private:
	static Pool *_s_instance;

private:
	ObjectPool<Symbol> _symbols;

	ObjectPool<MachineInteger> _machine_integers;
	ObjectPool<BigInteger> _big_integers;

	ObjectPool<BigRational> _big_rationals;

	ObjectPool<MachineReal> _machine_reals;
	ObjectPool<BigReal> _big_reals;

	ObjectPool<MachineComplex> _machine_complexs;
    ObjectPool<BigComplex> _big_complexs;

	TinyExpressionHeap<MaxTinySliceSize> _static_expression_heap;
	ObjectPool<ExpressionImplementation<BigSlice>> _dynamic_expressions;

	ObjectPool<String> _strings;

private:
	ObjectPool<Cache> _caches;
	ObjectPool<RefsExtent> _refs_extents;

	ObjectPool<Match> _matches;
	UnsafeMatchRef _default_match;
    ObjectPool<DynamicOptionsProcessor> _dynamic_options_processors;

	ObjectPool<SymbolicForm> _symbolic_forms;
	UnsafeSymbolicFormRef _no_symbolic_form;

    SlotAllocator _slots;
    VectorAllocator<UnsafeBaseExpressionRef> _unsafe_ref_vector_allocator;
    VectorAllocator<BaseExpressionRef> _ref_vector_allocator;

	ObjectAllocator<VariableMap::value_type> _variable_map;
    ObjectAllocator<OptionsMap::value_type> _options_map;
	ObjectAllocator<SymbolStateMap::value_type> _symbol_state_map;
    ObjectAllocator<MonomialMap::value_type> _monomial_map;
    ObjectAllocator<SymbolRulesMap::value_type> _rules_map_allocator;

public:
	static inline auto &variable_map_allocator() {
		return _s_instance->_variable_map;
	}

    static inline auto &options_map_allocator() {
        return _s_instance->_options_map;
    }

	static inline auto &symbol_state_map_allocator() {
		return _s_instance->_symbol_state_map;
	}

    static inline auto &monomial_map_allocator() {
        return _s_instance->_monomial_map;
    }

	static inline auto &slots_allocator() {
		return _s_instance->_slots;
	}

    static inline auto &unsafe_ref_vector_allocator() {
        return _s_instance->_unsafe_ref_vector_allocator;
    }

    static inline auto &safe_ref_vector_allocator() {
        return _s_instance->_ref_vector_allocator;
    }

    static inline auto &rules_map_allocator() {
        return _s_instance->_rules_map_allocator;
    }

public:
    static void init();

	static inline SymbolRef Symbol(const char *name, ExtendedType type);

	static inline BaseExpressionRef MachineInteger(machine_integer_t value);
    static inline BaseExpressionRef BigInteger(const mpz_class &value);
	static inline BaseExpressionRef BigInteger(mpz_class &&value);

	static inline BaseExpressionRef MachineRational(machine_integer_t x, machine_integer_t y);
	static inline BaseExpressionRef BigRational(const mpq_class &value);

	static inline BaseExpressionRef MachineReal(machine_real_t value);
    static inline BaseExpressionRef MachineReal(const SymbolicFormRef &form);

    static inline BaseExpressionRef BigReal(machine_real_t value, const Precision &prec);
	static inline BaseExpressionRef BigReal(mpfr_t value, const Precision &prec);

    static inline BaseExpressionRef MachineComplex(machine_real_t real, machine_real_t imag);
    static inline BaseExpressionRef BigComplex(const SymEngineComplexRef &value);

    static inline TinyExpressionRef<0> EmptyExpression(const BaseExpressionRef &head);

	template<int N>
	static inline TinyExpressionRef<N> TinyExpression(
		const BaseExpressionRef &head,
		TinySlice<N> &&slice) {

		assert(_s_instance);
		return _s_instance->_static_expression_heap.allocate<N>(head, std::move(slice));
	}

	template<typename Generator>
	static inline auto TinyExpression(
		const BaseExpressionRef &head,
		const Generator &generator) {

		assert(_s_instance);
		return _s_instance->_static_expression_heap.construct_from_generator(head, generator);
	}

	static inline BigExpressionRef Expression(const BaseExpressionRef &head, const BigSlice &slice);

    template<typename U>
    static inline PackedExpressionRef<U> Expression(const BaseExpressionRef &head, const PackedSlice<U> &slice) {
        return PackedExpressionRef<U>(new ExpressionImplementation<PackedSlice<U>>(head, slice));
    }

	static inline StringRef String(std::string &&utf8);

	static inline StringRef String(const StringExtentRef &extent);

	static inline StringRef String(const StringExtentRef &extent, size_t offset, size_t length);

	static inline CacheRef new_cache();

	static inline RefsExtentRef RefsExtent(const std::vector<BaseExpressionRef> &data);

	static inline RefsExtentRef RefsExtent(std::vector<BaseExpressionRef> &&data);

	static inline RefsExtentRef RefsExtent(const std::initializer_list<BaseExpressionRef> &data);

	static inline MatchRef Match(const PatternMatcherRef &matcher);

    static inline MatchRef Match(const PatternMatcherRef &matcher, const OptionsProcessorRef &options_processor);

	static inline MatchRef DefaultMatch();

    static inline OptionsProcessorRef DynamicOptionsProcessor();

	static inline SymbolicFormRef SymbolicForm(const SymEngineRef &ref) {
		return _s_instance->_symbolic_forms.construct(ref);
	}

	static inline SymbolicFormRef NoSymbolicForm() {
		return _s_instance->_no_symbolic_form;
	}


public:
	static inline void release(BaseExpression *expr);

	static inline void release(class RefsExtent *extent);

	static inline void release(Cache *cache);

	static inline void release(class Match *match) {
		_s_instance->_matches.destroy(match);
	}

    static inline void release(class OptionsProcessor *processor) {
        switch (processor->type) {
            case OptionsProcessor::Dynamic:
                _s_instance->_dynamic_options_processors.destroy(
                    static_cast<class DynamicOptionsProcessor*>(processor));
                break;
            case OptionsProcessor::Static:
                // StaticOptionsMatchers are always to be allocated on
                // the stack! so nothing to do here.
                break;
            default:
                throw std::runtime_error("illegal OptionsProcessor type");
        }
    }

	static inline void release(class SymbolicForm *form) {
		return _s_instance->_symbolic_forms.destroy(form);
	}
};

#endif //CMATHICS_HEAP_H
