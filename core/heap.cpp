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
            const SliceCode code = static_cast<const class Expression*>(expr)->slice_code();
            if (is_static_slice(code)) {
	            _s_instance->_static_expression_heap.free(expr, code);
            } else if (code == SliceCode::DynamicSliceCode) {
                _s_instance->_expression_refs.free(
                    static_cast<ExpressionImplementation<DynamicSlice>*>(expr));
            } else if (is_packed_slice(code)) {
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

BaseExpressionRef Heap::BigInteger(mpz_class &&value) {
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

DynamicExpressionRef Heap::Expression(const BaseExpressionRef &head, const DynamicSlice &slice) {
    assert(_s_instance);
    return DynamicExpressionRef(_s_instance->_expression_refs.construct(head, slice));
}
