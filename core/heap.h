#ifndef CMATHICS_HEAP_H
#define CMATHICS_HEAP_H

#include <boost/pool/object_pool.hpp>
#include <mpfrcxx/mpreal.h>
#include <static_if/static_if.hpp>

#include "gmpxx.h"

#include "cache.h"

class MachineInteger;
class BigInteger;
class Rational;

class MachineReal;
class BigReal;

template<size_t N>
class StaticSlice;

template<typename U>
class PackedSlice;

template<typename Slice>
class ExpressionImplementation;

template<typename U>
using PackedExpression = ExpressionImplementation<PackedSlice<U>>;

template<typename U>
using PackedExpressionRef = boost::intrusive_ptr<ExpressionImplementation<PackedSlice<U>>>;

template<size_t N>
using StaticExpression = ExpressionImplementation<StaticSlice<N>>;

template<size_t N>
using StaticExpressionRef = boost::intrusive_ptr<ExpressionImplementation<StaticSlice<N>>>;

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
	std::function<std::tuple<ExpressionRef, BaseExpressionRef*, TypeMask*>(const BaseExpressionRef &head)>
	MakeLateInit;

	void *_pool[UpToSize + 1];
	MakeFromVector _make_from_vector[UpToSize + 1];
	MakeFromInitializerList _make_from_initializer_list[UpToSize + 1];
	MakeLateInit _make_late_init[UpToSize + 1];
	std::function<void(BaseExpression*)> _free[UpToSize + 1];

	template<int N>
	void initialize() {
		const auto pool = new boost::object_pool<ExpressionImplementation<StaticSlice<N>>>;

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
				return std::tuple_cat(std::make_tuple(expr), expr->_leaves.late_init());
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
				StaticExpressionRef<N> expr = pool->construct(head, StaticSlice<N>());
				return std::tuple_cat(std::make_tuple(expr), expr->_leaves.late_init());
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
	inline StaticExpressionHeap() {
		initialize<UpToSize>();
	}

	template<int N>
	StaticExpressionRef<N> allocate(const BaseExpressionRef &head, const StaticSlice<N> &slice) {
		static_assert(N <= UpToSize, "N must not be be greater than UpToSize");
		return StaticExpressionRef<N>(static_cast<boost::object_pool<ExpressionImplementation<StaticSlice<N>>>*>(
  		    _pool[N])->construct(head, slice));
	}

	ExpressionRef make(const BaseExpressionRef &head, const std::vector<BaseExpressionRef> &leaves) {
		assert(leaves.size() <= UpToSize);
		return _make_from_vector[leaves.size()](head, leaves);
	}

	ExpressionRef make(const BaseExpressionRef &head, const std::initializer_list<BaseExpressionRef> &leaves) {
		assert(leaves.size() <= UpToSize);
		return _make_from_initializer_list[leaves.size()](head, leaves);
	}

	std::tuple<ExpressionRef, BaseExpressionRef*, TypeMask*> make_late_init(const BaseExpressionRef &head, size_t N) {
		assert(N <= UpToSize);
		return _make_late_init[N](head);
	}

	inline void free(BaseExpression *expr, SliceCode slice_id) const {
		const size_t i = static_slice_size(slice_id);
		assert(i <= UpToSize);
		_free[i](expr);
	}
};

class Heap {
private:
	static Heap *_s_instance;

	boost::object_pool<Symbol> _symbols;

	boost::object_pool<MachineInteger> _machine_integers;
    boost::object_pool<BigInteger> _big_integers;
	boost::object_pool<Rational> _rationals;

    boost::object_pool<MachineReal> _machine_reals;
    boost::object_pool<BigReal> _big_reals;

	StaticExpressionHeap<MaxStaticSliceSize> _static_expression_heap;

    boost::object_pool<ExpressionImplementation<DynamicSlice>> _expression_refs;

	boost::object_pool<Cache> _caches;

	Heap();

public:
    static void init();

    static void release(BaseExpression *expr);

	static SymbolRef Symbol(const char *name, ExtendedType type = SymbolExtendedType);

    static BaseExpressionRef MachineInteger(machine_integer_t value);
    static BaseExpressionRef BigInteger(const mpz_class &value);
	static BaseExpressionRef BigInteger(mpz_class &&value);
	static BaseExpressionRef Rational(machine_integer_t x, machine_integer_t y);

    static BaseExpressionRef MachineReal(machine_real_t value);
    static BaseExpressionRef BigReal(const mpfr::mpreal &value);
    static BaseExpressionRef BigReal(double prec, machine_real_t value);

	static StaticExpressionRef<0> EmptyExpression(const BaseExpressionRef &head);

	template<int N>
	static inline StaticExpressionRef<N> StaticExpression(
		const BaseExpressionRef &head,
		const StaticSlice<N> &slice) {

		assert(_s_instance);
		return _s_instance->_static_expression_heap.allocate<N>(head, slice);
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

	static inline std::tuple<ExpressionRef, BaseExpressionRef*, TypeMask*> StaticExpression(
		const BaseExpressionRef &head,
		size_t N) {

		assert(_s_instance);
		return _s_instance->_static_expression_heap.make_late_init(head, N);
	}

	static DynamicExpressionRef Expression(const BaseExpressionRef &head, const DynamicSlice &slice);

    template<typename U>
    static PackedExpressionRef<U> Expression(const BaseExpressionRef &head, const PackedSlice<U> &slice) {
        return PackedExpressionRef<U>(new ExpressionImplementation<PackedSlice<U>>(head, slice));
    }

	static Cache *new_cache() {
		return _s_instance->_caches.construct();
	}

	static void release_cache(Cache *cache) {
		_s_instance->_caches.free(cache);
	}
};

inline BaseExpressionRef from_primitive(machine_integer_t value) {
    return BaseExpressionRef(Heap::MachineInteger(value));
}

static_assert(sizeof(long) == sizeof(machine_integer_t),
	"types long and machine_integer_t must not differ");

inline BaseExpressionRef from_primitive(const mpz_class &value) {
	if (value.fits_slong_p()) {
		return from_primitive(static_cast<machine_integer_t>(value.get_si()));
	} else {
		return BaseExpressionRef(Heap::BigInteger(value));
	}
}

inline BaseExpressionRef from_primitive(mpz_class &&value) {
    if (value.fits_slong_p()) {
        return from_primitive(static_cast<machine_integer_t>(value.get_si()));
    } else {
        return BaseExpressionRef(Heap::BigInteger(value));
    }
}

#endif //CMATHICS_HEAP_H
