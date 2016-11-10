#ifndef CMATHICS_HEAP_H
#define CMATHICS_HEAP_H

#include <boost/pool/object_pool.hpp>
#include <mpfrcxx/mpreal.h>

#include "gmpxx.h"

class MachineInteger;
class BigInteger;

class MachineReal;
class BigReal;

template<size_t N>
class InPlaceRefsSlice;

template<typename U>
class PackSlice;

template<typename Slice>
class ExpressionImplementation;

template<typename U>
using PackExpressionRef = boost::intrusive_ptr<ExpressionImplementation<PackSlice<U>>>;

template<size_t N>
using InPlaceExpressionRef = boost::intrusive_ptr<ExpressionImplementation<InPlaceRefsSlice<N>>>;

class Heap {
private:
    static Heap *_s_instance;

    boost::object_pool<MachineInteger> _machine_integers;
    boost::object_pool<BigInteger> _big_integers;

    boost::object_pool<MachineReal> _machine_reals;
    boost::object_pool<BigReal> _big_reals;

    boost::object_pool<ExpressionImplementation<InPlaceRefsSlice<0>>> _expression0;
    boost::object_pool<ExpressionImplementation<InPlaceRefsSlice<1>>> _expression1;
    boost::object_pool<ExpressionImplementation<InPlaceRefsSlice<2>>> _expression2;
    boost::object_pool<ExpressionImplementation<InPlaceRefsSlice<3>>> _expression3;

    boost::object_pool<ExpressionImplementation<RefsSlice>> _expression_refs;

public:
    static void init();

    static void release(BaseExpression *expr);

    static BaseExpressionRef MachineInteger(machine_integer_t value);
    static BaseExpressionRef BigInteger(const mpz_class &value);

    static BaseExpressionRef MachineReal(machine_real_t value);
    static BaseExpressionRef BigReal(const mpfr::mpreal &value);
    static BaseExpressionRef BigReal(double prec, machine_real_t value);

	static InPlaceExpressionRef<0> EmptyExpression0(const BaseExpressionRef &head);
	static InPlaceExpressionRef<1> EmptyExpression1(const BaseExpressionRef &head);
	static InPlaceExpressionRef<2> EmptyExpression2(const BaseExpressionRef &head);
	static InPlaceExpressionRef<3> EmptyExpression3(const BaseExpressionRef &head);

	static InPlaceExpressionRef<0> Expression(const BaseExpressionRef &head, const InPlaceRefsSlice<0> &slice);
    static InPlaceExpressionRef<1> Expression(const BaseExpressionRef &head, const InPlaceRefsSlice<1> &slice);
    static InPlaceExpressionRef<2> Expression(const BaseExpressionRef &head, const InPlaceRefsSlice<2> &slice);
    static InPlaceExpressionRef<3> Expression(const BaseExpressionRef &head, const InPlaceRefsSlice<3> &slice);

    static RefsExpressionRef Expression(const BaseExpressionRef &head, const RefsSlice &slice);

    template<typename U>
    static PackExpressionRef<U> Expression(const BaseExpressionRef &head, const PackSlice<U> &slice) {
        return PackExpressionRef<U>(new ExpressionImplementation<PackSlice<U>>(head, slice));
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
