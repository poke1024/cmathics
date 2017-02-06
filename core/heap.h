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
class StaticSlice;

template<typename U>
class PackedSlice;

template<typename Slice>
class ExpressionImplementation;

template<typename U>
using PackedExpression = ExpressionImplementation<PackedSlice<U>>;

template<typename U>
using PackedExpressionRef = ConstSharedPtr<ExpressionImplementation<PackedSlice<U>>>;

template<int N>
using StaticExpression = ExpressionImplementation<StaticSlice<N>>;

template<int N>
using StaticExpressionRef = ConstSharedPtr<ExpressionImplementation<StaticSlice<N>>>;

struct SymbolHash {
	inline std::size_t operator()(const Symbol *symbol) const;

	inline std::size_t operator()(const SymbolRef &symbol) const;
};

class SymbolState;

struct SymbolEqual {
	inline bool operator()(const Symbol* lhs, const Symbol *rhs) const {
		return lhs == rhs;
	}

	inline bool operator()(const Symbol* lhs, const SymbolRef &rhs) const {
		return lhs == rhs.get();
	}

	inline bool operator()(const SymbolRef &lhs, const SymbolRef &rhs) const {
		return lhs == rhs;
	}

	inline bool operator()(const SymbolRef &lhs, const Symbol *rhs) const {
		return lhs.get() == rhs;
	}
};

class SymbolKey {
private:
    const SymbolRef _symbol;
    const char * const _name;

public:
    inline SymbolKey(SymbolRef symbol) : _symbol(symbol), _name(nullptr) {
    }

    inline SymbolKey(const char *name) : _name(name) {
    }

    inline int compare(const SymbolKey &key) const;

    inline bool operator==(const SymbolKey &key) const;

    inline bool operator<(const SymbolKey &key) const {
        return compare(key) < 0;
    }

    inline const char *c_str() const;
};

namespace std {
    // for the following hash, also see http://stackoverflow.com/questions/98153/
    // whats-the-best-hashing-algorithm-to-use-on-a-stl-string-when-using-hash-map

    template<>
    struct hash<SymbolKey> {
        inline size_t operator()(const SymbolKey &key) const {
            const char *s = key.c_str();

            size_t hash = 0;
            while (*s) {
                hash = hash * 101 + *s++;
            }

            return hash;
        }
    };
}

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
	inline BaseExpressionRef apply(
		const std::vector<Entry> &entries,
		const Expression *expr,
		Filter &filter,
		const Evaluation &evaluation) const;

	void insert_rule(
		std::vector<Entry> &entries,
		const Entry &entry);

	template<typename Expression, typename Filter>
	inline BaseExpressionRef apply(
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

	inline BaseExpressionRef try_apply(
		const Expression *expr, const Evaluation &evaluation) const;

	inline const auto &key() const;

	inline const auto &pattern() const;

	void merge(const RuleEntry &entry) {
		*this = entry;
	}
};

class Rules : public RulesVector<RuleEntry> {
public:
	using RulesVector<RuleEntry>::RulesVector;

	template<typename Expression>
	inline BaseExpressionRef apply(
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
	const Forms m_forms;

public:
	const RuleRef &rule() const {
		return m_rule;
	}

	FormatRule(const RuleRef &rule, const SymbolRef &form) :
		m_rule(rule), m_forms(&form, &form + 1) {
	}

	FormatRule(const RuleRef &rule, const Forms &forms) :
		m_rule(rule), m_forms(forms) {
	}

	FormatRuleRef merge(FormatRule *rule) {
		assert(m_rule.get() == rule->m_rule.get());
		Forms forms(m_forms);
		for (const SymbolRef &symbol : rule->m_forms) {
			forms.insert(symbol);
		}
		return FormatRuleRef(new FormatRule(m_rule, forms));
	}

	inline bool has_form(const SymbolRef &form) const {
		return m_forms.find(form) != m_forms.end();
	}
};

class FormatRuleEntry : public RuleHash {
private:
	UnsafeFormatRuleRef m_rule;

public:
	using RuleRef = ::FormatRuleRef;

	inline FormatRuleEntry(
		const FormatRuleRef &rule);

	inline BaseExpressionRef try_apply(
		const Expression *expr, const Evaluation &evaluation) const;

	inline const auto &key() const;

	inline const auto &pattern() const;

	void merge(const FormatRuleEntry &entry) {
		m_rule = m_rule->merge(entry.m_rule.get());
	}

	inline bool has_form(const SymbolRef &form) const {
		return m_rule->has_form(form);
	}
};

class FormatRules : public RulesVector<FormatRuleEntry> {
public:
	using RulesVector<FormatRuleEntry>::RulesVector;

	template<typename Expression>
	inline BaseExpressionRef apply(
		const Expression *expr,
		const SymbolRef &form,
		const Evaluation &evaluation) const;
};

template<typename Key, typename Value>
using SymbolMap = std::unordered_map<Key, Value,
	SymbolHash, SymbolEqual,
	ObjectAllocator<std::pair<const Key, Value>>>;

template<typename Value>
using SymbolPtrMap = SymbolMap<const Symbol*, Value>;

template<typename Value>
using SymbolRefMap = SymbolMap<const SymbolRef, Value>;

using VariableMap = SymbolPtrMap<const BaseExpressionRef*>;

using SymbolStateMap = SymbolRefMap<SymbolState>;

using MonomialMap = std::map<SymbolKey, size_t>;

using SymbolRulesMap = SymbolRefMap<Rules>;

class Cache;

typedef ConstSharedPtr<Cache> CacheRef;
typedef QuasiConstSharedPtr<Cache> CachedCacheRef;
typedef UnsafeSharedPtr<Cache> UnsafeCacheRef;

#include "pattern.h"

template<typename Generator, int UpToSize>
class StaticExpressionFactory {
public:
	std::function<ExpressionRef(void *, const BaseExpressionRef&, const Generator&)> m_construct[UpToSize + 1];

	template<int N>
	void initialize() {
		m_construct[N] = [] (void *pool, const BaseExpressionRef &head, const Generator &f) {
			auto *tpool = reinterpret_cast<ObjectPool<ExpressionImplementation<StaticSlice<N>>>*>(pool);
			return ExpressionRef(tpool->construct(head, f));
		};

		STATIC_IF (N >= 1) {
           initialize<N-1>();
       } STATIC_ENDIF
	}

public:
	inline StaticExpressionFactory() {
		initialize<UpToSize>();
	}

	inline ExpressionRef operator()(void * const *pools, const BaseExpressionRef& head, const Generator &f) const {
		const size_t n = f.size();
		return m_construct[n](pools[n], head, f);
	}
};

template<int UpToSize>
class StaticExpressionHeap {
private:
	typedef
	std::function<ExpressionRef(const BaseExpressionRef &head, const std::vector<BaseExpressionRef> &leaves)>
	MakeFromVector;

	typedef
	std::function<ExpressionRef(const BaseExpressionRef &head, const std::initializer_list<BaseExpressionRef> &leaves)>
	MakeFromInitializerList;

	typedef
	std::function<std::tuple<ExpressionRef, BaseExpressionRef*, std::atomic<TypeMask>*>(const BaseExpressionRef &head)>
	MakeLateInit;

	void *_pool[UpToSize + 1];
	MakeFromVector _make_from_vector[UpToSize + 1];
	MakeFromInitializerList _make_from_initializer_list[UpToSize + 1];
	MakeLateInit _make_late_init[UpToSize + 1];

	std::function<void(BaseExpression*)> _free[UpToSize + 1];

	template<int N>
	void initialize() {
		const auto pool = new ObjectPool<ExpressionImplementation<StaticSlice<N>>>();

		_pool[N] = pool;

		if (N == 0) {
			_make_from_vector[N] = [pool] (
				const BaseExpressionRef &head,
				const std::vector<BaseExpressionRef> &leaves) {

				return pool->construct(head);
			};
			_make_from_initializer_list[N] = [pool] (
				const BaseExpressionRef &head,
				const std::initializer_list<BaseExpressionRef> &leaves) {

				return pool->construct(head);
			};
			_make_late_init[N] = [pool] (const BaseExpressionRef &head) {
				StaticExpressionRef<N> expr = pool->construct(head);
				return std::tuple_cat(std::make_tuple(expr), const_cast<StaticSlice<N>&>(expr->_leaves).late_init());
			};
		} else {
			_make_from_vector[N] = [pool] (
				const BaseExpressionRef &head,
				const std::vector<BaseExpressionRef> &leaves) {

				return pool->construct(head, StaticSlice<N>(leaves));
			};
			_make_from_initializer_list[N] = [pool] (
				const BaseExpressionRef &head,
				const std::initializer_list<BaseExpressionRef> &leaves) {

				return pool->construct(head, StaticSlice<N>(leaves));
			};
			_make_late_init[N] = [pool] (const BaseExpressionRef &head) {
				StaticExpressionRef<N> expr = pool->construct(head);
				return std::tuple_cat(std::make_tuple(expr), const_cast<StaticSlice<N>&>(expr->_leaves).late_init());
			};
		}

		_free[N] = [pool] (BaseExpression *expr) {
			pool->destroy(static_cast<ExpressionImplementation<StaticSlice<N>>*>(expr));
		};

		STATIC_IF (N >= 1) {
			initialize<N-1>();
		} STATIC_ENDIF
	}

public:
	inline StaticExpressionHeap() {
		initialize<UpToSize>();
	}

	template<int N>
	StaticExpressionRef<N> allocate(const BaseExpressionRef &head, StaticSlice<N> &&slice) {
		static_assert(N <= UpToSize, "N must not be be greater than UpToSize");
		return StaticExpressionRef<N>(static_cast<ObjectPool<ExpressionImplementation<StaticSlice<N>>>*>(
  		    _pool[N])->construct(head, std::move(slice)));
	}

	inline ExpressionRef construct(const BaseExpressionRef &head, const std::vector<BaseExpressionRef> &leaves) {
		assert(leaves.size() <= UpToSize);
		return _make_from_vector[leaves.size()](head, leaves);
	}

	template<typename Generator>
	inline auto construct_from_generator(const BaseExpressionRef& head, const Generator &generator) const {
		static StaticExpressionFactory<Generator, UpToSize> factory;
		return factory(_pool, head, generator);
	}

	inline void destroy(BaseExpression *expr, SliceCode slice_id) const {
		const size_t i = static_slice_size(slice_id);
		assert(i <= UpToSize);
		_free[i](expr);
	}
};

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

	StaticExpressionHeap<MaxStaticSliceSize> _static_expression_heap;
	ObjectPool<ExpressionImplementation<DynamicSlice>> _dynamic_expressions;

	ObjectPool<String> _strings;

private:
	ObjectPool<Cache> _caches;
	ObjectPool<RefsExtent> _refs_extents;

	ObjectPool<Match> _matches;
	UnsafeMatchRef _default_match;

	ObjectPool<SymbolicForm> _symbolic_forms;
	UnsafeSymbolicFormRef _no_symbolic_form;

    SlotAllocator _slots;
    VectorAllocator<UnsafeBaseExpressionRef> _ref_vector_allocator;

	ObjectAllocator<VariableMap::value_type> _variable_map;
	ObjectAllocator<SymbolStateMap::value_type> _symbol_state_map;
    ObjectAllocator<MonomialMap::value_type> _monomial_map;
    ObjectAllocator<SymbolRulesMap::value_type> _rules_map_allocator;

public:
	static inline auto &variable_map_allocator() {
		return _s_instance->_variable_map;
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

    static inline auto &ref_vector_allocator() {
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

	static inline BaseExpressionRef BigRational(machine_integer_t x, machine_integer_t y);
	static inline BaseExpressionRef BigRational(const mpq_class &value);

	static inline BaseExpressionRef MachineReal(machine_real_t value);
    static inline BaseExpressionRef MachineReal(const SymbolicFormRef &form);

    static inline BaseExpressionRef BigReal(machine_real_t value, const Precision &prec);
	static inline BaseExpressionRef BigReal(const SymbolicFormRef &form, const Precision &prec);
	static inline BaseExpressionRef BigReal(arb_t value, const Precision &prec);

    static inline BaseExpressionRef MachineComplex(machine_real_t real, machine_real_t imag);
    static inline BaseExpressionRef BigComplex(const SymEngineComplexRef &value);

    static inline StaticExpressionRef<0> EmptyExpression(const BaseExpressionRef &head);

	template<int N>
	static inline StaticExpressionRef<N> StaticExpression(
		const BaseExpressionRef &head,
		StaticSlice<N> &&slice) {

		assert(_s_instance);
		return _s_instance->_static_expression_heap.allocate<N>(head, std::move(slice));
	}

	template<typename Generator>
	static inline auto StaticExpression(
		const BaseExpressionRef &head,
		const Generator &generator) {

		assert(_s_instance);
		return _s_instance->_static_expression_heap.construct_from_generator(head, generator);
	}

	static inline DynamicExpressionRef Expression(const BaseExpressionRef &head, const DynamicSlice &slice);

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

	static inline MatchRef DefaultMatch();

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

	static inline void release(class SymbolicForm *form) {
		return _s_instance->_symbolic_forms.destroy(form);
	}
};

#endif //CMATHICS_HEAP_H
