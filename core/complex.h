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

    virtual std::string fullform() const {
        std::ostringstream s;
        const machine_real_t imag = value.imag();
        s << std::showpoint << std::setprecision(6) << value.real() << (imag >= 0. ? " + " : " - ") << imag << " I";
        return s.str();
    }

    virtual bool match(const BaseExpression &expr) const {
        return same(expr);
    }

protected:
    virtual inline SymbolicFormRef instantiate_symbolic_form() const final {
        return Pool::SymbolicForm(SymEngine::complex_double(value));
    }
};

/*
class BigComplex : public BaseExpression {
public:
    static constexpr Type Type = BigComplexType;

    acb_t value;
    const Precision prec;

    explicit inline BigComplex(acb_t value_, const Precision &prec_) :
        BaseExpression(BigRealExtendedType), prec(prec_) {

        arb_swap(value, value_);
    }
};
*/

#endif //CMATHICS_COMPLEX_H
