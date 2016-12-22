#ifndef CMATHICS_HEAP_H
#define CMATHICS_HEAP_H

#include <boost/pool/object_pool.hpp>
#include <static_if/static_if.hpp>

#include <boost/pool/pool_alloc.hpp>
#include <boost/unordered/unordered_map.hpp>
#include <forward_list>

#include "gmpxx.h"
#include <arb.h>

class MachineInteger;
class BigInteger;

class BigRational;

class Precision;
class MachineReal;
class BigReal;

class RefsExtent;
typedef SharedPtr<RefsExtent> RefsExtentRef;

class StringExtent;
typedef SharedPtr<StringExtent> StringExtentRef;

template<int N>
class StaticSlice;

template<typename U>
class PackedSlice;

template<typename Slice>
class ExpressionImplementation;

template<typename U>
using PackedExpression = ExpressionImplementation<PackedSlice<U>>;

template<typename U>
using PackedExpressionRef = SharedPtr<ExpressionImplementation<PackedSlice<U>>>;

template<int N>
using StaticExpression = ExpressionImplementation<StaticSlice<N>>;

template<int N>
using StaticExpressionRef = SharedPtr<ExpressionImplementation<StaticSlice<N>>>;

struct SymbolHash {
	inline std::size_t operator()(const Symbol *symbol) const;
};

template<typename Value>
using VariableMap = boost::unordered_map<const Symbol*, Value,
	SymbolHash, std::equal_to<const Symbol*>,
	boost::fast_pool_allocator<std::pair<const Symbol*, Value>>>;

using VariablePtrMap = VariableMap<const BaseExpressionRef*>;

class Cache;

typedef SharedPtr<Cache> CacheRef;

#include "pattern.h"

template<int UpToSize>
class StaticExpressionHeap {
private:
    const size_t m_chunk_size;

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
		const auto pool = new boost::object_pool<ExpressionImplementation<StaticSlice<N>>>(m_chunk_size);

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
			pool->free(static_cast<ExpressionImplementation<StaticSlice<N>>*>(expr));
		};

		STATIC_IF (N >= 1) {
			initialize<N-1>();
		} STATIC_ENDIF
	}

public:
	inline StaticExpressionHeap(size_t chunk_size) : m_chunk_size(chunk_size) {
		initialize<UpToSize>();
	}

	template<int N>
	StaticExpressionRef<N> allocate(const BaseExpressionRef &head, StaticSlice<N> &&slice) {
		static_assert(N <= UpToSize, "N must not be be greater than UpToSize");
		return StaticExpressionRef<N>(static_cast<boost::object_pool<ExpressionImplementation<StaticSlice<N>>>*>(
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

	inline void free(BaseExpression *expr, SliceCode slice_id) const {
		const size_t i = static_slice_size(slice_id);
		assert(i <= UpToSize);
		_free[i](expr);
	}
};

class Pool {
private:
	static /*thread_local*/ Pool *_s_instance;

	boost::object_pool<Symbol> _symbols;

	boost::object_pool<MachineInteger> _machine_integers;
    boost::object_pool<BigInteger> _big_integers;

	boost::object_pool<BigRational> _big_rationals;

    boost::object_pool<MachineReal> _machine_reals;
    boost::object_pool<BigReal> _big_reals;

	StaticExpressionHeap<MaxStaticSliceSize> _static_expression_heap;
    boost::object_pool<ExpressionImplementation<DynamicSlice>> _dynamic_expressions;

	boost::object_pool<String> _strings;

	boost::object_pool<Cache> _caches;
	boost::object_pool<RefsExtent> _refs_extents;

	boost::object_pool<Match> _matches;
	MatchRef _default_match;

	boost::object_pool<SymbolicForm> _symbolic_forms;
	SymbolicFormRef _no_symbolic_form;

	boost::fast_pool_allocator<VariablePtrMap::value_type> _variable_ptr_map;
	SlotAllocator _slots;

    Pool();

public:
	static inline auto variable_ptr_map_allocator() {
		return _s_instance->_variable_ptr_map;
	}

	static inline auto slots_allocator() {
		return _s_instance->_slots;
	}

public:
    static void init();

    static inline void release(BaseExpression *expr);

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

	static inline void release(Cache *cache);

	static inline RefsExtentRef RefsExtent(const std::vector<BaseExpressionRef> &data);

	static inline RefsExtentRef RefsExtent(std::vector<BaseExpressionRef> &&data);

	static inline RefsExtentRef RefsExtent(const std::initializer_list<BaseExpressionRef> &data);

	static inline void release(class RefsExtent *extent);

	static inline MatchRef Match(const PatternMatcherRef &matcher);

	static inline MatchRef DefaultMatch();

	static inline void release(class Match *match) {
		_s_instance->_matches.free(match);
	}

	static inline SymbolicFormRef SymbolicForm(const SymEngineRef &ref, bool is_simplified = false) {
		return _s_instance->_symbolic_forms.construct(ref, is_simplified);
	}

	static inline SymbolicFormRef NoSymbolicForm() {
		return _s_instance->_no_symbolic_form;
	}

	static inline void release(class SymbolicForm *form) {
		return _s_instance->_symbolic_forms.free(form);
	}
};

#endif //CMATHICS_HEAP_H
