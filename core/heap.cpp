#include "types.h"
#include "heap.h"
#include "integer.h"
#include "leaves.h"
#include "expression.h"
#include "definitions.h"
#include "matcher.h"

Heap *Heap::_s_instance = nullptr;


Heap::Heap() {
}

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
            if (is_in_place_slice(type_id)) {
	            _s_instance->_free_static_expression(expr, type_id);
            } else if (type_id == SliceTypeId::RefsSliceCode) {
                _s_instance->_expression_refs.free(
                    static_cast<ExpressionImplementation<DynamicSlice>*>(expr));
            } else if (is_pack_slice(type_id)) {
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

StaticExpressionRef<0> Heap::EmptyExpression(const BaseExpressionRef &head) {
	return StaticExpression<0>(head, EmptySlice());
}

/*StaticExpressionRef<0> Heap::StaticExpression(const BaseExpressionRef &head, const StaticSlice<0> &slice) {
    assert(_s_instance);
    return StaticExpressionRef<0>(_s_instance->_expression0.construct(head));
}

StaticExpressionRef<1> Heap::StaticExpression(const BaseExpressionRef &head, const StaticSlice<1> &slice) {
    assert(_s_instance);
    return StaticExpressionRef<1>(_s_instance->_expression1.construct(head, slice));
}

StaticExpressionRef<2> Heap::StaticExpression(const BaseExpressionRef &head, const StaticSlice<2> &slice) {
    assert(_s_instance);
    return StaticExpressionRef<2>(_s_instance->_expression2.construct(head, slice));
}

StaticExpressionRef<3> Heap::StaticExpression(const BaseExpressionRef &head, const StaticSlice<3> &slice) {
    assert(_s_instance);
    return StaticExpressionRef<3>(_s_instance->_expression3.construct(head, slice));
}*/

DynamicExpressionRef Heap::Expression(const BaseExpressionRef &head, const DynamicSlice &slice) {
    assert(_s_instance);
    return DynamicExpressionRef(_s_instance->_expression_refs.construct(head, slice));
}
