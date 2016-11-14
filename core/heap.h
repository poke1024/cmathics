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

template<int N>
struct StaticExpressionPool {
	static boost::object_pool<ExpressionImplementation<StaticSlice<N>>> pool;
};

template<int UpToSize>
class FreeStaticExpression {
private:
	void *_pools[UpToSize + 1];
	std::function<void(BaseExpression*)> _alloc[UpToSize + 1];
	std::function<void(BaseExpression*)> _free[UpToSize + 1];

	template<int N>
	void fill() {
		_pools[N] = new boost::object_pool<ExpressionImplementation<StaticSlice<N>>>;

		_free[N] = [](BaseExpression *expr) {
			StaticExpressionPool<N>::pool.free(
				static_cast<ExpressionImplementation<StaticSlice<N>>*>(expr));
		};
		STATIC_IF (N >= 1) {
			fill<N-1>();
		} STATIC_ENDIF
	}

public:
	inline FreeStaticExpression() {
		fill<UpToSize>();
	}

	inline void operator()(BaseExpression *expr, SliceTypeId slice_id) const {
		const size_t i = in_place_slice_size(slice_id);
		assert(i <= UpToSize);
		_functions[i](expr);
	}
};

class Heap {
private:
	static Heap *_s_instance;

    boost::object_pool<MachineInteger> _machine_integers;
    boost::object_pool<BigInteger> _big_integers;

    boost::object_pool<MachineReal> _machine_reals;
    boost::object_pool<BigReal> _big_reals;

	FreeStaticExpression<3> _free_static_expression;

    boost::object_pool<ExpressionImplementation<DynamicSlice>> _expression_refs;

	Heap();

public:
    static void init();

    static void release(BaseExpression *expr);

    static BaseExpressionRef MachineInteger(machine_integer_t value);
    static BaseExpressionRef BigInteger(const mpz_class &value);

    static BaseExpressionRef MachineReal(machine_real_t value);
    static BaseExpressionRef BigReal(const mpfr::mpreal &value);
    static BaseExpressionRef BigReal(double prec, machine_real_t value);

	static StaticExpressionRef<0> EmptyExpression(const BaseExpressionRef &head);

	template<int N>
	static StaticExpressionRef<N> StaticExpression(const BaseExpressionRef &head, const StaticSlice<N> &slice) {
		assert(_s_instance);
		return StaticExpressionRef<N>(StaticExpressionPool<N>::pool.construct(head, slice));
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
