#ifndef CMATHICS_COMPLEX_H
#define CMATHICS_COMPLEX_H

#include <stdint.h>
#include <sstream>
#include <iomanip>

#include <arb.h>
#include <symengine/eval_arb.h>
#include <symengine/real_double.h>
#include <symengine/real_mpfr.h>

#include "types.h"
#include "hash.h"

class MachineComplex : public BaseExpression {
private:
    static const std::hash<machine_real_t> hash_function;

public:
    static constexpr Type Type = MachineComplexType;

    const std::complex<machine_real_t> value;

    explicit MachineComplex(machine_real_t real, machine_real_t imag) :
        BaseExpression(MachineComplexExtendedType), value(real, imag) {
    }

    virtual BaseExpressionPtr head(const Evaluation &evaluation) const final;

    virtual inline bool same(const BaseExpression &expr) const final {
        if (expr.type() == MachineComplexType) {
            return value == static_cast<const MachineComplex*>(&expr)->value;
        } else {
            return false;
        }
    }

    virtual hash_t hash() const {
        // TODO better hash see CPython _Py_HashDouble
        const hash_t value_hash = hash_pair(
            hash_function(value.real()), hash_function(value.imag()));
        return hash_pair(machine_complex_hash, value_hash);
    }

    virtual std::string format(const SymbolRef &form, const Evaluation &evaluation) const final;

    virtual bool match(const BaseExpression &expr) const {
        return same(expr);
    }

    virtual bool is_inexact() const final {
        return true;
    }

protected:
    virtual inline SymbolicFormRef instantiate_symbolic_form() const final {
        return Pool::SymbolicForm(SymEngine::complex_double(value), true);
    }
};

class BigComplex : public BaseExpression {
public:
    static constexpr Type Type = BigComplexType;

    //acb_t value;
    //const Precision prec;

    SymEngineComplexRef m_value;

    explicit inline BigComplex(const SymEngineComplexRef &value) :
        BaseExpression(BigComplexExtendedType), m_value(value) {
    }

    virtual bool same(const BaseExpression &expr) const {
        if (expr.type() == BigComplexType) {
            return m_value->__eq__(*static_cast<const BigComplex*>(&expr)->m_value.get());
        } else {
            return false;
        }
    }

    virtual bool equals(const BaseExpression &expr) const {
        return same(expr);
    }

    virtual hash_t hash() const {
        return m_value->__hash__();
    }

    virtual std::string format(const SymbolRef &form, const Evaluation &evaluation) const final;

    virtual BaseExpressionPtr head(const Evaluation &evaluation) const final;

    virtual bool is_inexact() const final {
        return false; // SymEngine's complex uses rationals
    }
};

#endif //CMATHICS_COMPLEX_H
