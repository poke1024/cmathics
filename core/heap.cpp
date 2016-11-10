#include "types.h"
#include "heap.h"
#include "integer.h"
#include "leaves.h"
#include "expression.h"
#include "definitions.h"
#include "matcher.h"

Heap *Heap::_s_instance = nullptr;

void Heap::init() {
    assert(_s_instance == nullptr);
    _s_instance = new Heap();
}

void Heap::release(BaseExpression *expr) {
    switch (expr->type()) {
        case MachineIntegerType:
            _s_instance->_machine_integers.free(static_cast<class MachineInteger*>(expr));
            break;

        case BigIntegerType:
            _s_instance->_big_integers.free(static_cast<class BigInteger*>(expr));
            break;

        case MachineRealType:
            _s_instance->_machine_reals.free(static_cast<class MachineReal*>(expr));
            break;

        case BigRealType:
            _s_instance->_big_reals.free(static_cast<class BigReal*>(expr));
            break;

        case ExpressionType: {
            const SliceTypeId type_id = static_cast<const class Expression*>(expr)->slice_type_id();
            if (is_in_place(type_id)) {
                switch (in_place_size(type_id)) {
                    case 0:
                        _s_instance->_expression0.free(
                            static_cast<ExpressionImplementation<InPlaceRefsSlice<0>>*>(expr));
                        break;
                    case 1:
                        _s_instance->_expression1.free(
                            static_cast<ExpressionImplementation<InPlaceRefsSlice<1>>*>(expr));
                        break;
                    case 2:
                        _s_instance->_expression2.free(
                            static_cast<ExpressionImplementation<InPlaceRefsSlice<2>>*>(expr));
                        break;
                    case 3:
                        _s_instance->_expression3.free(
                            static_cast<ExpressionImplementation<InPlaceRefsSlice<3>>*>(expr));
                        break;
                    default:
                        throw std::runtime_error("encountered unsupported in-place-slice size");
                }
            } else if (type_id == SliceTypeId::RefsSliceCode) {
                _s_instance->_expression_refs.free(
                    static_cast<ExpressionImplementation<RefsSlice>*>(expr));
            } else if (type_id == SliceTypeId::PackSliceCode) {
                delete expr;
            } else {
                throw std::runtime_error("encountered unsupported slice type id");
            }
            break;
        }

        default:
            delete expr;
            break;
    }
}

BaseExpressionRef Heap::MachineInteger(machine_integer_t value) {
    assert(_s_instance);
    return BaseExpressionRef(_s_instance->_machine_integers.construct(value));
}

BaseExpressionRef Heap::BigInteger(const mpz_class &value) {
    assert(_s_instance);
    return BaseExpressionRef(_s_instance->_big_integers.construct(value));
}

BaseExpressionRef Heap::MachineReal(machine_real_t value) {
    assert(_s_instance);
    return BaseExpressionRef(_s_instance->_machine_reals.construct(value));
}

BaseExpressionRef Heap::BigReal(const mpfr::mpreal &value) {
    assert(_s_instance);
    return BaseExpressionRef(_s_instance->_big_reals.construct(value));
}

BaseExpressionRef Heap::BigReal(double prec, machine_real_t value) {
    assert(_s_instance);
    return BaseExpressionRef(_s_instance->_big_reals.construct(prec, value));
}

InPlaceExpressionRef<0> Heap::EmptyExpression0(const BaseExpressionRef &head) {
    assert(_s_instance);
    return InPlaceExpressionRef<0>(_s_instance->_expression0.construct(head));
}

InPlaceExpressionRef<1> Heap::EmptyExpression1(const BaseExpressionRef &head) {
    assert(_s_instance);
    return InPlaceExpressionRef<1>(_s_instance->_expression1.construct(head));
}

InPlaceExpressionRef<2> Heap::EmptyExpression2(const BaseExpressionRef &head) {
    assert(_s_instance);
    return InPlaceExpressionRef<2>(_s_instance->_expression2.construct(head));
}

InPlaceExpressionRef<3> Heap::EmptyExpression3(const BaseExpressionRef &head) {
    assert(_s_instance);
    return InPlaceExpressionRef<3>(_s_instance->_expression3.construct(head));
}

InPlaceExpressionRef<0> Heap::Expression(const BaseExpressionRef &head, const InPlaceRefsSlice<0> &slice) {
    assert(_s_instance);
    return InPlaceExpressionRef<0>(_s_instance->_expression0.construct(head));
}

InPlaceExpressionRef<1> Heap::Expression(const BaseExpressionRef &head, const InPlaceRefsSlice<1> &slice) {
    assert(_s_instance);
    return InPlaceExpressionRef<1>(_s_instance->_expression1.construct(head, slice));
}

InPlaceExpressionRef<2> Heap::Expression(const BaseExpressionRef &head, const InPlaceRefsSlice<2> &slice) {
    assert(_s_instance);
    return InPlaceExpressionRef<2>(_s_instance->_expression2.construct(head, slice));
}

InPlaceExpressionRef<3> Heap::Expression(const BaseExpressionRef &head, const InPlaceRefsSlice<3> &slice) {
    assert(_s_instance);
    return InPlaceExpressionRef<3>(_s_instance->_expression3.construct(head, slice));
}

RefsExpressionRef Heap::Expression(const BaseExpressionRef &head, const RefsSlice &slice) {
    assert(_s_instance);
    return RefsExpressionRef(_s_instance->_expression_refs.construct(head, slice));
}
