#ifndef CMATHICS_HEAP_H
#define CMATHICS_HEAP_H

#include <static_if/static_if.hpp>

#include <forward_list>

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

class Cache;

typedef ConstSharedPtr<Cache> CacheRef;
typedef QuasiConstSharedPtr<Cache> CachedCacheRef;
typedef UnsafeSharedPtr<Cache> UnsafeCacheRef;

#include "pattern.h"

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
  		    _pool[N])->construct(head, slice));
	}

	inline ExpressionRef make(const BaseExpressionRef &head, const std::vector<BaseExpressionRef> &leaves) {
		assert(leaves.size() <= UpToSize);
		return _make_from_vector[leaves.size()](head, leaves);
	}

	inline ExpressionRef make(const BaseExpressionRef &head, const std::initializer_list<BaseExpressionRef> &leaves) {
		assert(leaves.size() <= UpToSize);
		return _make_from_initializer_list[leaves.size()](head, leaves);
	}

	inline auto make_late_init(const BaseExpressionRef &head, size_t N) {
		assert(N <= UpToSize);
		return _make_late_init[N](head);
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

	ObjectAllocator<VariableMap::value_type> _variable_map;
	ObjectAllocator<SymbolStateMap::value_type> _symbol_state_map;
	SlotAllocator _slots;

public:
	static inline auto &variable_map_allocator() {
		return _s_instance->_variable_map;
	}

	static inline auto &symbol_state_map_allocator() {
		return _s_instance->_symbol_state_map;
	}

	static inline auto &slots_allocator() {
		return _s_instance->_slots;
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
    static inline BaseExpressionRef BigReal(machine_real_t value, const Precision &prec);
	static inline BaseExpressionRef BigReal(const SymbolicFormRef &form, const Precision &prec);
	static inline BaseExpressionRef BigReal(arb_t value, const Precision &prec);

    static inline BaseExpressionRef MachineComplex(machine_real_t real, machine_real_t imag);

    static inline StaticExpressionRef<0> EmptyExpression(const BaseExpressionRef &head);

	template<int N>
	static inline StaticExpressionRef<N> StaticExpression(
		const BaseExpressionRef &head,
		StaticSlice<N> &&slice) {

		assert(_s_instance);
		return _s_instance->_static_expression_heap.allocate<N>(head, std::move(slice));
	}

	static inline ExpressionRef StaticExpression(
		const BaseExpressionRef &head,
		const std::vector<BaseExpressionRef> &leaves) {

		assert(_s_instance);
		return _s_instance->_static_expression_heap.make(head, leaves);
	}

	static inline ExpressionRef StaticExpression(
		const BaseExpressionRef &head,
		const std::initializer_list<BaseExpressionRef> &leaves) {

		assert(_s_instance);
		return _s_instance->_static_expression_heap.make(head, leaves);
	}

	static inline auto StaticExpression(
		const BaseExpressionRef &head,
		size_t N) {

		assert(_s_instance);
		return _s_instance->_static_expression_heap.make_late_init(head, N);
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

	static inline SymbolicFormRef SymbolicForm(const SymEngineRef &ref, bool is_simplified = false) {
		return _s_instance->_symbolic_forms.construct(ref, is_simplified);
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
