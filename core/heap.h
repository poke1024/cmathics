#ifndef CMATHICS_HEAP_H
#define CMATHICS_HEAP_H

#include <boost/pool/object_pool.hpp>
#include <mpfrcxx/mpreal.h>
#include <static_if/static_if.hpp>

#include "gmpxx.h"

class MachineInteger;
class BigInteger;

class MachineReal;
class BigReal;

template<size_t N>
class StaticSlice;

template<typename U>
class PackedSlice;

template<typename Slice>
class ExpressionImplementation;

template<typename U>
using PackedExpressionRef = boost::intrusive_ptr<ExpressionImplementation<PackedSlice<U>>>;

template<size_t N>
using StaticExpressionRef = boost::intrusive_ptr<ExpressionImplementation<StaticSlice<N>>>;

template<int UpToSize>
class StaticExpressionHeap {
private:
	void *_pool[UpToSize + 1];
	std::function<void(BaseExpression*)> _free[UpToSize + 1];

	template<int N>
	void initialize() {
		const auto pool = new boost::object_pool<ExpressionImplementation<StaticSlice<N>>>;

		_pool[N] = pool;

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
		static_assert(N <= UpToSize, "N must be be greater than UpToSize");
		return StaticExpressionRef<N>(static_cast<boost::object_pool<ExpressionImplementation<StaticSlice<N>>>*>(
  		    _pool[N])->construct(head, slice));
	}

	inline void free(BaseExpression *expr, SliceTypeId slice_id) const {
		const size_t i = in_place_slice_size(slice_id);
		assert(i <= UpToSize);
		_free[i](expr);
	}
};

class Heap {
private:
	static Heap *_s_instance;

    boost::object_pool<MachineInteger> _machine_integers;
    boost::object_pool<BigInteger> _big_integers;

    boost::object_pool<MachineReal> _machine_reals;
    boost::object_pool<BigReal> _big_reals;

	StaticExpressionHeap<3> _static_expression_heap;

    boost::object_pool<ExpressionImplementation<DynamicSlice>> _expression_refs;

	Heap();

public:
    static void init();

    static void release(BaseExpression *expr);

    static BaseExpressionRef MachineInteger(machine_integer_t value);
    static BaseExpressionRef BigInteger(const mpz_class &value);

    static BaseExpressionRef MachineReal(machine_real_t vTalue);
    static BaseExpressionRef BigReal(const mpfr::mpreal &value);
    static BaseExpressionRef BigReal(double prec, machine_real_t value);

	static StaticExpressionRef<0> EmptyExpression(const BaseExpressionRef &head);

	template<int N>
	static StaticExpressionRef<N> StaticExpression(const BaseExpressionRef &head, const StaticSlice<N> &slice) {
		assert(_s_instance);
		return _s_instance->_static_expression_heap.allocate<N>(head, slice);
	}

    static DynamicExpressionRef Expression(const BaseExpressionRef &head, const DynamicSlice &slice);

    template<typename U>
    static PackedExpressionRef<U> Expression(const BaseExpressionRef &head, const PackedSlice<U> &slice) {
        return PackedExpressionRef<U>(new ExpressionImplementation<PackedSlice<U>>(head, slice));
    }
};

inline BaseExpressionRef from_primitive(machine_integer_t value) {
    return BaseExpressionRef(Heap::MachineInteger(value));
}

inline BaseExpressionRef from_primitive(const mpz_class &value) {
    static_assert(sizeof(long) == sizeof(machine_integer_t), "types long and machine_integer_t differ");
    if (value.fits_slong_p()) {
        return from_primitive(static_cast<machine_integer_t>(value.get_si()));
    } else {
        return BaseExpressionRef(Heap::BigInteger(value));
    }
}

#endif //CMATHICS_HEAP_H
